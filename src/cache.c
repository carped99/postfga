#include <postgres.h>
#include <utils/hsearch.h>
#include <utils/memutils.h>
#include <storage/lwlock.h>
#include <storage/shmem.h>

#include <xxhash.h>

#include "postfga.h"
#include "cache.h"

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------*/
static bool postfga_cache_acl_lookup(HTAB *ht, const FgaAclCacheKey *key, uint16_t generation, uint64_t now_ms, bool *allowed_out);
static bool postfga_cache_acl_store(HTAB *ht, const FgaAclCacheKey *key, uint16_t generation, uint64_t now_ms, uint64_t ttl_ms, bool allowed);

/*-------------------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------------------*/
FgaAclCacheKey
postfga_make_check_key(const FgaCheckTupleRequest *req)
{
    FgaAclCacheKey key;

    char buf[2048]; /* 상당히 넉넉하게 */

    // store_id|object_type|object_id|subject_type|subject_id|relation
    int len = snprintf(buf, sizeof(buf),
                       "%s|%s|%s|%s|%s|%s",
                       req->store_id,
                       req->tuple.object_type,
                       req->tuple.object_id,
                       req->tuple.subject_type,
                       req->tuple.subject_id,
                       req->tuple.relation);

    if (len < 0)
        len = 0;
    if (len > (int)sizeof(buf))
        len = sizeof(buf);

    {
        XXH128_hash_t h = XXH3_128bits(buf, (size_t)len);
        key.low = h.low64;
        key.high = h.high64;
    }

    return key;
}

void postfga_l1_init(FgaL1Cache *cache, MemoryContext parent_ctx, long size_hint)
{
    HASHCTL ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaAclCacheKey);
    ctl.entrysize = sizeof(FgaAclCacheEntry);
    ctl.hcxt = AllocSetContextCreate(parent_ctx,
                                     "PostFGA L1 Cache",
                                     ALLOCSET_SMALL_SIZES);

    cache->acl = hash_create("PostFGA L1 Cache",
                             size_hint > 0 ? size_hint : 1024,
                             &ctl,
                             HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

    cache->ctx = ctl.hcxt;
    cache->generation = 0;
}

bool postfga_l1_lookup(FgaL1Cache *cache,
                       const FgaAclCacheKey *key,
                       uint16_t cur_generation,
                       uint64_t now_ms,
                       bool *allowed_out)
{
    if (cache->acl == NULL)
        return false;

    return postfga_cache_acl_lookup(cache->acl, key, cur_generation, now_ms, allowed_out);
}

void postfga_l1_store(FgaL1Cache *cache,
                      const FgaAclCacheKey *key,
                      uint16_t generation,
                      uint64_t now_ms,
                      uint64_t ttl_ms,
                      bool allowed)
{
    if (cache->acl == NULL)
        return;
    postfga_cache_acl_store(cache->acl, key, generation, now_ms, ttl_ms, allowed);
}

bool postfga_l2_lookup(FgaL2Cache *cache,
                       const FgaAclCacheKey *key,
                       uint16_t cur_generation,
                       uint64_t now_ms,
                       bool *allowed_out)
{
    bool found = false;

    if (cache->acl != NULL)
    {
        LWLockAcquire(cache->lock, LW_SHARED);

        found = postfga_cache_acl_lookup(cache->acl, key, cur_generation, now_ms, allowed_out);

        LWLockRelease(cache->lock);
    }

    return found;
}

void postfga_l2_store(FgaL2Cache *cache,
                      const FgaAclCacheKey *key,
                      uint16_t generation,
                      uint64_t now_ms,
                      uint64_t ttl_ms,
                      bool allowed)
{
    if (cache->acl == NULL)
        return;

    LWLockAcquire(cache->lock, LW_EXCLUSIVE);
    postfga_cache_acl_store(cache->acl, key, generation, now_ms, ttl_ms, allowed);
    LWLockRelease(cache->lock);
}

static bool postfga_cache_acl_lookup(HTAB *ht, const FgaAclCacheKey *key, uint16_t generation, uint64_t now_ms, bool *allowed_out)
{
    bool found;
    FgaAclCacheEntry *e = (FgaAclCacheEntry *)hash_search(ht,
                                                          (const void *)key,
                                                          HASH_FIND,
                                                          &found);

    if (!found)
        return false;

    /* generation mismatch → stale */
    if (e->generation != generation)
        return false;

    /* TTL 만료 */
    if (e->expires_at_ms != 0 && e->expires_at_ms < now_ms)
        return false;

    *allowed_out = e->allowed;

    return true;
}

static bool postfga_cache_acl_store(HTAB *ht,
                                    const FgaAclCacheKey *key,
                                    uint16_t generation,
                                    uint64_t now_ms,
                                    uint64_t ttl_ms,
                                    bool allowed)
{
    bool found;
    FgaAclCacheEntry *e = (FgaAclCacheEntry *)hash_search(ht,
                                                          (const void *)key,
                                                          HASH_ENTER,
                                                          &found);

    e->key = *key;
    e->allowed = allowed;
    e->generation = generation;
    e->expires_at_ms = (ttl_ms > 0) ? (now_ms + ttl_ms) : 0;

    return found;
}