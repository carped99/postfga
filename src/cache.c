#include <postgres.h>

#include <access/xact.h>
#include <storage/lwlock.h>
#include <storage/shmem.h>
#include <utils/hsearch.h>
#include <utils/memutils.h>
#include <varatt.h>
#include <xxhash.h>

#include "cache.h"
#include "cache_key.h"
#include "cache_l1.h"
#include "cache_l2.h"
#include "postfga.h"

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


Size postfga_cache_shmem_base_size(void)
{
    Size capacity = l2_capacity_from_config();

    /* cache structure */
    Size size = offsetof(FgaL2AclCache, entries);

    /* entries 배열 */
    size = add_size(size, mul_size(sizeof(FgaL2AclEntry), capacity));

    return size;
}

Size postfga_cache_shmem_hash_size(void)
{
    Size capacity = l2_capacity_from_config();
    Size hash_size = l2_hash_size(capacity);

    return hash_estimate_size(hash_size, sizeof(FgaL2AclSlot));
}

void postfga_cache_shmem_init(FgaL2AclCache* cache, LWLock* lock)
{
    Size capacity = l2_capacity_from_config();
    /* initialize cache struct */
    MemSet(cache, 0, offsetof(FgaL2AclCache, entries));
    cache->lock = lock;
    cache->capacity = capacity;
    cache->generation = 0;
    pg_atomic_init_u32(&cache->nextVictim, 0);

    // entries 초기화
    for (uint32 i = 0; i < cache->capacity; i++)
        cache->entries[i].valid = false;
}

void postfga_cache_shmem_each_startup(void)
{
    l1_startup();
    l2_startup();
}

bool postfga_cache_lookup(const FgaAclCacheKey* key, uint64_t ttl_ms, bool* allowed_out)
{
    PostfgaShmemState* state;
    FgaL2AclCache* l2;
    TimestampTz now_ms;
    TimestampTz expires_at;

    PostfgaConfig* config = postfga_get_config();
    if (!config->cache_enabled)
        return false;

    state = postfga_get_shmem_state();
    l2 = l2_cache();

    now_ms = get_now_ms();

    if (l1_lookup(key, l2->generation, now_ms, allowed_out))
    {
        cache_hit(&state->stats.l1_cache);
        return true;
    }
    cache_miss(&state->stats.l1_cache);

    if (l2_lookup(l2, key, now_ms, allowed_out, &expires_at))
    {
        // L1 캐시에 복사
        l1_store(key, l2->generation, expires_at, *allowed_out);
        cache_hit(&state->stats.l2_cache);
        return true;
    }
    cache_miss(&state->stats.l2_cache);
    return false;
}

void postfga_cache_store(const FgaAclCacheKey* key, uint64_t ttl_ms, bool allowed)
{
    FgaL2AclCache* l2;
    TimestampTz now_ms;
    TimestampTz expires_at;

    PostfgaConfig* config = postfga_get_config();
    if (!config->cache_enabled)
        return;

    l2 = l2_cache();
    now_ms = get_now_ms();
    expires_at = now_ms + ttl_ms;

    l1_store(key, l2->generation, expires_at, allowed);
    l2_store(l2, key, now_ms, expires_at, allowed);
}
