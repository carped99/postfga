#include <postgres.h>

#include <access/xact.h>
#include <storage/lwlock.h>
#include <storage/shmem.h>
#include <utils/hsearch.h>
#include <utils/memutils.h>
#include <varatt.h>
#include <xxhash.h>

#include "cache.h"
#include "config.h"
#include "postfga.h"
#include "shmem.h"

#define FGA_L2_USAGE_MAX 5
#define FGA_L2_HASH_NAME "postfga L2 index"

typedef struct FgaAclCacheValue
{
    bool allowed;

    uint16_t global_gen; /* generation mismatch 시 invalid */
    uint16_t object_gen; /* generation mismatch 시 invalid */

    TimestampTz expires_at_ms; /* TTL 기준 만료 시간 (epoch ms) */
    uint8 usage_count;         /* clock-sweep usage count */
} FgaAclCacheValue;

/* ACL Cache Entry */
typedef struct FgaAclCacheEntry
{
    FgaAclCacheKey key;
    FgaAclCacheValue value;
    bool valid;
} FgaAclCacheEntry;

typedef struct FgaL1Cache
{
    MemoryContext ctx; /* 캐시 엔트리용 컨텍스트 */
    HTAB* acl;         /* key: FgaCheckKey, entry: FgaCacheEntry */
    HTAB* relation;    /* relation name -> bit index */
    uint16 generation; /* L2 generation을 미러링할 때 사용 */
    uint32 max_entries;
} FgaL1Cache;

typedef struct FgaL2SlotEntry
{
    FgaAclCacheKey key;
    uint32 slot_no; /* entries[slot_no] */
} FgaL2SlotEntry;

typedef struct FgaL2Cache
{
    LWLock* lock;                /* 8 shared cache lock */
    pg_atomic_uint32 nextVictim; /* 4 clock hand index 0..capacity-1 */
    uint32 capacity;             /* 4 number of slots in entries[] */
    uint16 generation;           /* 2 global generation for invalidation */
    char _pad[6];                /* padding for 32-byte alignment */
    FgaAclCacheEntry entries[FLEXIBLE_ARRAY_MEMBER];
} FgaL2Cache;

static FgaL1Cache backend_l1_cache;
static bool backend_l1_inited = false;
static HTAB* l2_hash_table = NULL;
static Size l2_capacity_ = 0;
static Size l2_hash_size_ = 0;

/*-------------------------------------------------------------------------
 * Cache shared memory initialization
 *-------------------------------------------------------------------------*/
static Size l2_capacity()
{
    PostfgaConfig* config = postfga_get_config();

    /* MB → bytes */
    Size bytes = (Size)config->cache_size * 1024 * 1024;
    Size per = sizeof(FgaAclCacheEntry);

    return bytes / per;
}

Size postfga_cache_shmem_base_size(void)
{
    l2_capacity_ = l2_capacity();
    l2_hash_size_ = l2_capacity_ * 2;

    /* cache structure */
    Size size = offsetof(FgaL2Cache, entries);

    /* entries 배열 */
    size = add_size(size, mul_size(sizeof(FgaAclCacheEntry), l2_capacity_));

    return size;
}

Size postfga_cache_shmem_hash_size(void)
{
    return hash_estimate_size(l2_hash_size_, sizeof(FgaL2SlotEntry));
}

void postfga_cache_shmem_init(FgaL2Cache* cache, LWLock* lock)
{
    /* initialize cache struct */
    MemSet(cache, 0, offsetof(FgaL2Cache, entries));
    cache->lock = lock;
    cache->capacity = l2_capacity_;
    cache->generation = 0;
    pg_atomic_init_u32(&cache->nextVictim, 0);

    // entries 초기화
    for (uint32 i = 0; i < cache->capacity; i++)
        cache->entries[i].valid = false;

    // {
    //     const Size size = postfga_cache_shmem_size();
    //     /* 초기화 정보 로그 */
    //     const double cache_mb = (double)size / (1024.0 * 1024.0);
    //     const double entry_kb = sizeof(FgaAclCacheEntry);
    //     const double hash_mb = hash_estimate_size(l2_hash_size_, sizeof(FgaL2SlotEntry)) / (1024.0 * 1024.0);
    //     const double total_mb = cache_mb + hash_mb;

    //     ereport(LOG,
    //             (errmsg("postfga: L2 cache initialized"),
    //              errdetail("Cache slots: %u (%.1f B each, %.2f MB total)\n"
    //                        "Hash table: %u max entries (%.2f MB)\n"
    //                        "Total memory: %.2f MB\n"
    //                        "Load factor: %.1f%% when full",
    //                        l2_capacity_,
    //                        entry_kb,
    //                        cache_mb,
    //                        l2_hash_size_,
    //                        hash_mb,
    //                        total_mb,
    //                        (double)l2_capacity_ / l2_hash_size_ * 100.0)));
    // }
}

void postfga_cache_shmem_hash_startup(void)
{
    HASHCTL ctl;
    bool found;

    if (l2_hash_table != NULL)
        return;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaAclCacheKey);
    ctl.entrysize = sizeof(FgaL2SlotEntry);

    l2_hash_table = ShmemInitHash(FGA_L2_HASH_NAME,
                                  l2_hash_size_,
                                  l2_hash_size_,
                                  &ctl,
                                  HASH_ELEM | HASH_BLOBS | HASH_FIXED_SIZE | HASH_SHARED_MEM);
}

/*-------------------------------------------------------------------------
 * CacheKey helpers
 *-------------------------------------------------------------------------*/
static inline char* append_cache_field_text(char* buffer, text* str)
{
    size_t str_len = str != NULL ? VARSIZE_ANY_EXHDR(str) : 0;
    Assert(str_len <= 255);

    *buffer++ = (uint8_t)str_len;

    if (str_len > 0)
    {
        memcpy(buffer, VARDATA_ANY(str), str_len);
        buffer += str_len;
    }

    return buffer;
}

static inline char* append_cache_field(char* buffer, const char* str)
{
    size_t str_len = str != NULL ? strlen(str) : 0;
    Assert(str_len <= 128);

    *buffer++ = (uint8_t)str_len;

    if (str_len > 0)
    {
        memcpy(buffer, str, str_len);
        buffer += str_len;
    }

    return buffer;
}

void postfga_cache_key(FgaAclCacheKey* key,
                       const char* store_id,
                       const char* model_id,
                       text* object_type,
                       text* object_id,
                       text* subject_type,
                       text* subject_id,
                       text* relation)
{
    XXH128_hash_t h;
    char buf[1024]; // 각 필드 길이 128 바이트 이하 가정으로 충분
    char* p = buf;

    p = append_cache_field(p, store_id);
    p = append_cache_field(p, model_id);
    p = append_cache_field_text(p, object_type);
    p = append_cache_field_text(p, object_id);

    // object_key 생성
    key->object_key = XXH3_64bits(buf, p - buf);

    p = append_cache_field_text(p, subject_type);
    p = append_cache_field_text(p, subject_id);
    p = append_cache_field_text(p, relation);

    h = XXH3_128bits(buf, p - buf);
    key->low = h.low64;
    key->high = h.high64;
}

/*-------------------------------------------------------------------------
 * L1 Cache
 *-------------------------------------------------------------------------*/
static void l1_init(FgaL1Cache* cache, MemoryContext parent_ctx, long size_hint)
{
    HASHCTL ctl;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaAclCacheKey);
    ctl.entrysize = sizeof(FgaAclCacheEntry);
    ctl.hcxt = AllocSetContextCreate(parent_ctx, "PostFGA L1 Cache", ALLOCSET_SMALL_SIZES);

    cache->acl = hash_create("PostFGA L1 Cache", size_hint, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

    cache->ctx = ctl.hcxt;
    cache->generation = 0;
    cache->max_entries = size_hint;
}

static inline FgaL1Cache* l1_cache(void)
{
    if (!backend_l1_inited)
    {
        l1_init(&backend_l1_cache, TopMemoryContext, 1024);
        backend_l1_inited = true;
    }
    return &backend_l1_cache;
}

bool l1_lookup(
    FgaL1Cache* cache, const FgaAclCacheKey* key, uint16_t cur_generation, uint64_t now_ms, bool* allowed_out)
{
    bool found;
    FgaAclCacheEntry* entry;

    if (cache == NULL || cache->acl == NULL)
        return false;

    entry = (FgaAclCacheEntry*)hash_search(cache->acl, key, HASH_FIND, &found);
    if (!found)
    {
        return false;
    }

    /* TTL 만료면 제거하고 miss 처리 */
    if (entry->value.expires_at_ms <= now_ms)
    {
        (void)hash_search(cache->acl, key, HASH_REMOVE, NULL);
        return false;
    }

    /* 세대(global generation) 불일치 시도 miss + 제거 */
    if (entry->value.global_gen != cur_generation)
    {
        (void)hash_search(cache->acl, key, HASH_REMOVE, NULL);
        return false;
    }

    *allowed_out = entry->value.allowed;

    return true;
}

void l1_store(FgaL1Cache* cache, const FgaAclCacheKey* key, uint16_t generation, uint64_t expires_at, bool allowed)
{
    FgaAclCacheEntry* entry;
    bool found;

    if (cache == NULL || cache->acl == NULL)
        return;

    /* capacity 초과면 그냥 저장 안 함 */
    if (!hash_search(cache->acl, key, HASH_FIND, &found) && !found)
    {
        if (hash_get_num_entries(cache->acl) >= cache->max_entries)
            return;
    }

    entry = (FgaAclCacheEntry*)hash_search(cache->acl, key, HASH_ENTER, &found);
    if (entry == NULL)
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("postfga: could not allocate L1 cache entry")));

    entry->key = *key;
    entry->value.allowed = allowed;
    entry->value.expires_at_ms = expires_at;
    entry->value.global_gen = generation;
}


/*-------------------------------------------------------------------------
 * L2 Cache
 *-------------------------------------------------------------------------*/
static inline void l2_update_entry(
    FgaAclCacheEntry* entry, const FgaAclCacheKey* key, TimestampTz expires_at, bool allowed, uint16_t generation)
{
    entry->key = *key;
    entry->value.allowed = allowed;
    entry->value.expires_at_ms = expires_at;
    entry->value.global_gen = generation;
    entry->value.usage_count = FGA_L2_USAGE_MAX; /* 새로 갱신된 항목은 최대치로 시작 */
    entry->valid = true;
}

static inline bool l2_entry_expired(const FgaL2Cache* cache, const FgaAclCacheEntry* entry, TimestampTz now_ms)
{
    if (!entry->valid)
        return true;

    if (entry->value.expires_at_ms <= now_ms)
        return true;

    if (entry->value.global_gen != cache->generation)
        return true;

    return false;
}

static inline uint32 l2_clock_sweep(const FgaL2Cache* cache)
{
    // Atomically move hand ahead one slot
    uint32 victim = pg_atomic_fetch_add_u32(&cache->nextVictim, 1);

    // wrap around
    return victim % cache->capacity;
}

static inline uint32 l2_find_victim_slot(const FgaL2Cache* cache, TimestampTz now_ms)
{
    uint32 trycounter = cache->capacity;

    for (;;)
    {
        uint32 idx = l2_clock_sweep(cache);
        FgaAclCacheEntry* entry = &cache->entries[idx];

        /*
         * 1) 비어있거나(expired/invalid) → 즉시 victim.
         *    PG에서는 refcount == 0 && usage_count == 0 인 버퍼를 victim으로 삼는데,
         *    여기서 valid==false 또는 TTL/세대 만료는 "더 이상 쓸 수 없는 버퍼"이므로
         *    바로 교체 가능하다고 보는 것.
         */
        if (l2_entry_expired(cache, entry, now_ms))
        {
            entry->valid = false;
            return idx;
        }

        /*
         * 2) 아직 유효하지만 usage_count > 0 이면 PG처럼 usage_count-- 하고 continue.
         */
        if (entry->value.usage_count > 0)
        {
            entry->value.usage_count--;

            /* PG처럼 상태 변화가 있었으니 trycounter 재설정 */
            trycounter = cache->capacity;
        }
        else
        {
            /*
             * 3) usage_count == 0 이면서 아직 유효한 엔트리 → second chance 다 쓴 버퍼.
             *    StrategyGetBuffer에서 refcount==0 && usage_count==0 인 버퍼를 victim으로 고르는
             *    부분에 해당 (refcount는 여기선 항상 0으로 보는 셈).
             */
            entry->valid = false;
            return idx;
        }

        /*
         * 4) trycounter: PG에서 "모든 버퍼가 pinned인 상태로 NBuffers번 돌면 ERROR" 용.
         *    여기선 pin 개념은 없지만, 혹시나 이상한 상황에서 무한루프를 피하기 위한 안전장치로 둔다.
         */
        if (--trycounter == 0)
            return UINT32_MAX;
    }
}

static inline FgaL2Cache* l2_cache(void)
{
    return postfga_get_shmem_state()->cache;
}

bool l2_lookup(
    const FgaL2Cache* cache, const FgaAclCacheKey* key, TimestampTz now_ms, bool* allowed_out, TimestampTz* expires_at)
{
    bool found;
    FgaAclCacheEntry* entry;

    if (cache == NULL)
        return false;

    LWLockAcquire(cache->lock, LW_SHARED);

    entry = (FgaAclCacheEntry*)hash_search(l2_hash_table, key, HASH_FIND, &found);
    if (!found)
    {
        LWLockRelease(cache->lock);
        return false;
    }

    // expired 검사
    if (l2_entry_expired(cache, entry, now_ms))
    {
        /* expired → clock sweep에게 빨리 잡히도록 usage_count = 0 */
        entry->value.usage_count = 0;
        LWLockRelease(cache->lock);
        return false;
    }

    if (entry->value.usage_count < FGA_L2_USAGE_MAX)
        entry->value.usage_count++;

    *allowed_out = entry->value.allowed;
    *expires_at = entry->value.expires_at_ms;

    LWLockRelease(cache->lock);

    return found;
}

void l2_store(FgaL2Cache* cache, const FgaAclCacheKey* key, TimestampTz now_ms, TimestampTz expires_at, bool allowed)
{
    bool found;
    FgaL2SlotEntry* idx;
    FgaAclCacheEntry* entry;

    if (cache == NULL)
        return;

    LWLockAcquire(cache->lock, LW_EXCLUSIVE);

    /* 1. 기존 엔트리 업데이트 */
    idx = hash_search(l2_hash_table, key, HASH_FIND, &found);
    if (found)
    {
        entry = &cache->entries[idx->slot_no];
        l2_update_entry(entry, key, expires_at, allowed, cache->generation);
        LWLockRelease(cache->lock);
        return;
    }

    /* 2. find victim */
    uint32 victim_slot = l2_find_victim_slot(cache, now_ms);
    if (victim_slot == UINT32_MAX)
    {
        /* victim 못 찾으면 그냥 포기 (캐시 미사용) */
        LWLockRelease(cache->lock);
        return;
    }

    entry = &cache->entries[victim_slot];
    if (entry->valid)
    {
        hash_search(l2_hash_table, &entry->key, HASH_REMOVE, NULL);
    }

    /* 3. 새 엔트리 생성 */
    idx = hash_search(l2_hash_table, key, HASH_ENTER, &found);
    if (idx == NULL)
    {
        LWLockRelease(cache->lock);
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("postfga: could not allocate L2 cache index entry")));
    }
    Assert(!found);

    /* hash_search already filled in the key */
    idx->slot_no = victim_slot;

    l2_update_entry(entry, key, expires_at, allowed, cache->generation);

    LWLockRelease(cache->lock);
}


static inline TimestampTz get_now_ms(void)
{
    // TimestampTz tx_ts = GetCurrentTransactionStartTimestamp();
    // int64 us = (int64)(tx_ts - 0); /* absolute 기준 필요 없음 */
    // if (us < 0)
    //     us = 0;

    TimestampTz now = GetCurrentTimestamp();
    return now / 1000; // convert microseconds → ms
}

static inline void cache_hit(FgaCacheStats* stats)
{
    pg_atomic_fetch_add_u64(&stats->hits, 1);
}

static inline void cache_miss(FgaCacheStats* stats)
{
    pg_atomic_fetch_add_u64(&stats->misses, 1);
}

bool postfga_cache_lookup(const FgaAclCacheKey* key, uint64_t ttl_ms, bool* allowed_out)
{
    PostfgaShmemState* state;
    FgaL1Cache* l1;
    FgaL2Cache* l2;
    uint64_t now_ms;
    uint64_t expires_at;

    PostfgaConfig* config = postfga_get_config();
    if (!config->cache_enabled)
        return false;

    state = postfga_get_shmem_state();
    l1 = l1_cache();
    l2 = l2_cache();

    now_ms = get_now_ms();

    if (l1_lookup(l1, key, l2->generation, now_ms, allowed_out))
    {
        cache_hit(&state->stats.l1_cache);
        return true;
    }
    cache_miss(&state->stats.l1_cache);

    if (l2_lookup(l2, key, now_ms, allowed_out, &expires_at))
    {
        // L1 캐시에 복사
        l1_store(l1, key, l2->generation, expires_at, *allowed_out);
        cache_hit(&state->stats.l2_cache);
        return true;
    }
    cache_miss(&state->stats.l2_cache);
    return false;
}

void postfga_cache_store(const FgaAclCacheKey* key, uint64_t ttl_ms, bool allowed)
{
    FgaL1Cache* l1;
    FgaL2Cache* l2;
    TimestampTz now_ms;
    TimestampTz expires_at;

    PostfgaConfig* config = postfga_get_config();
    if (!config->cache_enabled)
        return;

    l1 = l1_cache();
    l2 = l2_cache();
    now_ms = get_now_ms();
    expires_at = now_ms + ttl_ms;

    l1_store(l1, key, l2->generation, expires_at, allowed);
    l2_store(l2, key, now_ms, expires_at, allowed);
}
