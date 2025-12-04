#include <postgres.h>

#include <access/xact.h>
#include <storage/lwlock.h>
#include <storage/shmem.h>
#include <utils/hsearch.h>
#include <utils/memutils.h>

#include <xxhash.h>

#include "cache.h"
#include "postfga.h"
#include "shmem.h"

/*-------------------------------------------------------------------------
 * Backend-local L1 cache
 *-------------------------------------------------------------------------*/
static FgaL1Cache    backend_l1_cache;
static bool          backend_l1_inited = false;

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------*/
static inline char *
append_cache_field(char *buffer, const char *str)
{
    size_t str_len = (str != NULL) ? strlen(str) : 0;
    Assert(str_len <= 255);

    *buffer++ = (uint8_t) str_len;
    
    if (str_len > 0)
    {
        memcpy(buffer, str, str_len);
        buffer += str_len;
    }
    return buffer;
}

FgaAclCacheKey postfga_cache_key(const char* store_id, 
                const char* auth_model_id,
                const char* object_type,
                const char* object_id,
                const char* subject_type,
                const char* subject_id,
                const char* relation)
{
    FgaAclCacheKey key;
    XXH128_hash_t h;
    char   buf[1024]; // 각 필드 길이 128 바이트 이하 가정으로 충분
    char  *p = buf;

    p = append_cache_field(p, store_id);
    p = append_cache_field(p, auth_model_id);
    p = append_cache_field(p, object_type);
    p = append_cache_field(p, object_id);

    // object_key 생성
    key.object_key = XXH3_64bits(buf, p - buf);

    p = append_cache_field(p, subject_type);
    p = append_cache_field(p, subject_id);
    p = append_cache_field(p, relation);

    h = XXH3_128bits(buf, p - buf);
    key.low  = h.low64;
    key.high = h.high64;
    return key;        
}

static inline uint64_t get_now_ms(void)
{
    TimestampTz tx_ts = GetCurrentTransactionStartTimestamp();
    int64       us    = (int64) (tx_ts - 0);   /* absolute 기준 필요 없음 */
    if (us < 0)
        us = 0;

    return (uint64_t) (us / 1000);  // convert microseconds → ms
}

static
void l1_init(FgaL1Cache* cache, MemoryContext parent_ctx, long size_hint)
{
    HASHCTL ctl;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaAclCacheKey);
    ctl.entrysize = sizeof(FgaAclCacheEntry);
    ctl.hcxt = AllocSetContextCreate(parent_ctx, "PostFGA L1 Cache", ALLOCSET_SMALL_SIZES);

    cache->acl = hash_create("PostFGA L1 Cache", 
        size_hint, 
        &ctl, 
        HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

    cache->ctx = ctl.hcxt;
    cache->generation = 0;
}

static FgaL1Cache *
l1_cache(void)
{
    if (!backend_l1_inited)
    {
        l1_init(&backend_l1_cache, TopMemoryContext, 1024);
        backend_l1_inited = true;
    }
    return &backend_l1_cache;
}

static bool l1_lookup(FgaL1Cache* cache, 
    const FgaAclCacheKey* key, 
    uint16_t cur_generation, 
    uint64_t now_ms, 
    bool* allowed_out)
{
    bool             found;
    FgaAclCacheEntry *entry;

    if (cache == NULL || cache->acl == NULL)
        return false;

    entry = (FgaAclCacheEntry *) hash_search(cache->acl, key, HASH_FIND, &found);
    if (!found)
        return false;

    /* TTL 만료면 제거하고 miss 처리 */
    if (entry->value.expires_at_ms <= now_ms)
    {
        (void) hash_search(cache->acl, key, HASH_REMOVE, NULL);
        return false;
    }

    /* 세대(global generation) 불일치 시도 miss + 제거 */
    if (entry->value.global_gen != cur_generation)
    {
        (void) hash_search(cache->acl, key, HASH_REMOVE, NULL);
        return false;
    }

    *allowed_out = entry->value.allowed;

    return true;
}

static void l1_store(FgaL1Cache* cache, 
    const FgaAclCacheKey* key, 
    uint16_t generation, 
    uint64_t now_ms, 
    uint64_t ttl_ms, 
    bool allowed)
{
    FgaAclCacheEntry *entry;
    bool              found;

    if (cache == NULL || cache->acl == NULL)
        return;

    entry = (FgaAclCacheEntry *) hash_search(cache->acl, key, HASH_ENTER, &found);
    if (entry == NULL)
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("postfga: could not allocate L1 cache entry")));

    entry->key                  = *key;
    entry->value.allowed        = allowed;
    entry->value.expires_at_ms  = now_ms + ttl_ms;
    entry->value.global_gen     = generation;
}

static bool l2_lookup(
    const FgaL2Cache* cache, 
    const FgaAclCacheKey* key, 
    uint64_t now_ms, 
    bool* allowed_out)
{
    bool found;
    FgaAclCacheEntry* entry;

    if (cache == NULL || cache->acl == NULL)
        return false;

    LWLockAcquire(cache->lock, LW_SHARED);
    entry = (FgaAclCacheEntry*)hash_search(cache->acl, key, HASH_FIND, &found);

    if (!found)
    {
        LWLockRelease(cache->lock);
        return false;
    }

    if (entry->value.expires_at_ms <= now_ms)
    {
        (void) hash_search(cache->acl, key, HASH_REMOVE, NULL);
        LWLockRelease(cache->lock);
        return false;
    }

    if (entry->value.global_gen != cache->generation)
    {
        LWLockRelease(cache->lock);
        return false;
    }

    *allowed_out = entry->value.allowed;

    LWLockRelease(cache->lock);

    return found;
}

static void l2_store(FgaL2Cache* cache, 
    const FgaAclCacheKey* key, 
    uint64_t now_ms, 
    uint64_t ttl_ms, 
    bool allowed)
{
    FgaAclCacheEntry *entry;
    bool              found;

    if (cache == NULL || cache->acl == NULL)
        return;

    LWLockAcquire(cache->lock, LW_EXCLUSIVE);

    entry = (FgaAclCacheEntry *) hash_search(cache->acl,
                                             key,
                                             HASH_ENTER,
                                             &found);

    if (entry == NULL)
    {
        LWLockRelease(cache->lock);
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("postfga: could not allocate L2 cache entry")));
    }

    entry->key = *key;
    entry->value.allowed       = allowed;
    entry->value.expires_at_ms = now_ms + ttl_ms;
    entry->value.global_gen = cache->generation;
    // entry->object_gen = fga_object_gen_get(key);

    LWLockRelease(cache->lock);
}

bool postfga_cache_lookup(const FgaAclCacheKey* key, uint64_t ttl_ms, bool* allowed_out)
{
    PostfgaShmemState* state = postfga_get_shmem_state();
    uint64_t now_ms = get_now_ms();

    FgaL1Cache* l1 = l1_cache();

    if (l1_lookup(l1, key, state->cache.generation, now_ms, allowed_out))
    {
        return true;
    }

    if (l2_lookup(&state->cache, key, now_ms, allowed_out))
    {
        // L1 캐시에 복사
        l1_store(l1, key, state->cache.generation, now_ms, ttl_ms, *allowed_out);
        return true;
    }
    return false;
}

void postfga_cache_store(const FgaAclCacheKey* key, uint64_t ttl_ms, bool allowed)
{
    PostfgaShmemState* state = postfga_get_shmem_state();
    uint64_t now_ms = get_now_ms();

    FgaL1Cache* l1 = l1_cache();

    l1_store(l1, key, state->cache.generation, now_ms, ttl_ms, allowed);
    l2_store(&state->cache, key, now_ms, ttl_ms, allowed);
}