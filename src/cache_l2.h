#ifndef POSTFGA_CACHE_L2_ACL_H
#define POSTFGA_CACHE_L2_ACL_H

#include <postgres.h>

#include "cache.h"
#include "config.h"
#include "shmem.h"

#define FGA_L2_USAGE_MAX 5
#define FGA_L2_HASH_NAME "postfga L2 index"

typedef struct FgaL2AclValue
{
    bool allowed;

    uint16_t global_gen; /* generation mismatch 시 invalid */
    uint16_t object_gen; /* generation mismatch 시 invalid */

    TimestampTz expires_at_ms; /* TTL 기준 만료 시간 (epoch ms) */
    uint8 usage_count;         /* clock-sweep usage count */
} FgaL2AclValue;

/* ACL Cache Entry */
typedef struct FgaL2AclEntry
{
    FgaAclCacheKey key;
    FgaL2AclValue value;
    bool valid;
} FgaL2AclEntry;

typedef struct FgaL2AclSlot
{
    FgaAclCacheKey key;
    uint32 slot_no; /* entries[slot_no] */
} FgaL2AclSlot;

typedef struct FgaL2AclCache
{
    LWLock* lock;                /* 8 shared cache lock */
    pg_atomic_uint32 nextVictim; /* 4 clock hand index 0..capacity-1 */
    uint32 capacity;             /* 4 number of slots in entries[] */
    uint16 generation;           /* 2 global generation for invalidation */
    char _pad[6];                /* padding for 32-byte alignment */
    FgaL2AclEntry entries[FLEXIBLE_ARRAY_MEMBER];
} FgaL2AclCache;

static HTAB* l2_slot_table = NULL;

static inline FgaL2AclCache* l2_cache(void)
{
    return postfga_get_shmem_state()->cache;
}

static Size l2_capacity_from_config()
{
    PostfgaConfig* config = postfga_get_config();

    /* MB → bytes */
    Size bytes = (Size)config->cache_size * 1024 * 1024;
    Size per = sizeof(FgaL2AclEntry);

    return bytes / per;
}

static Size l2_hash_size(Size capacity)
{
    return capacity * 2; // load factor 0.5
}

static inline void l2_update_entry(
    FgaL2AclEntry* entry, const FgaAclCacheKey* key, TimestampTz expires_at, bool allowed, uint16_t generation)
{
    entry->key = *key;
    entry->value.allowed = allowed;
    entry->value.expires_at_ms = expires_at;
    entry->value.global_gen = generation;
    entry->value.usage_count = FGA_L2_USAGE_MAX; /* 새로 갱신된 항목은 최대치로 시작 */
    entry->valid = true;
}

static inline bool l2_entry_expired(const FgaL2AclCache* cache, const FgaL2AclEntry* entry, TimestampTz now_ms)
{
    if (!entry->valid)
        return true;

    if (entry->value.expires_at_ms <= now_ms)
        return true;

    if (entry->value.global_gen != cache->generation)
        return true;

    return false;
}

static inline uint32 l2_clock_sweep(FgaL2AclCache* const cache)
{
    // Atomically move hand ahead one slot
    uint32 victim = pg_atomic_fetch_add_u32(&cache->nextVictim, 1);

    // wrap around
    return victim % cache->capacity;
}

static uint32 l2_find_victim_slot(FgaL2AclCache* const cache, TimestampTz now_ms)
{
    uint32 trycounter = cache->capacity;

    for (;;)
    {
        uint32 idx = l2_clock_sweep(cache);
        FgaL2AclEntry* entry = &cache->entries[idx];

        if (l2_entry_expired(cache, entry, now_ms))
            return idx;

        if (entry->value.usage_count > 0)
        {
            entry->value.usage_count--;
            trycounter = cache->capacity;
        }
        else
        {
            return idx;
        }

        if (--trycounter == 0)
            return UINT32_MAX;
    }
}

static void l2_startup(void)
{
    HASHCTL ctl;
    FgaL2AclCache* cache;
    Size hash_elems;

    if (l2_slot_table != NULL)
        return;

    cache = postfga_get_shmem_state()->cache;
    hash_elems = l2_hash_size(cache->capacity);

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaAclCacheKey);
    ctl.entrysize = sizeof(FgaL2AclSlot);

    l2_slot_table = ShmemInitHash(
        FGA_L2_HASH_NAME, hash_elems, hash_elems, &ctl, HASH_ELEM | HASH_BLOBS | HASH_FIXED_SIZE | HASH_SHARED_MEM);
}

static bool l2_lookup(
    FgaL2AclCache* cache, const FgaAclCacheKey* key, TimestampTz now_ms, bool* allowed_out, TimestampTz* expires_at)
{
    bool found;
    FgaL2AclSlot* slot;
    FgaL2AclEntry* entry;

    if (cache == NULL)
        return false;

    LWLockAcquire(cache->lock, LW_SHARED);

    slot = (FgaL2AclSlot*)hash_search(l2_slot_table, key, HASH_FIND, &found);
    if (!found)
    {
        LWLockRelease(cache->lock);
        return false;
    }

    entry = &cache->entries[slot->slot_no];

    if (l2_entry_expired(cache, entry, now_ms))
    {
        entry->value.usage_count = 0; /* victim 빨리 되게 */
        LWLockRelease(cache->lock);
        return false;
    }

    if (entry->value.usage_count < FGA_L2_USAGE_MAX)
        entry->value.usage_count++;

    *allowed_out = entry->value.allowed;
    *expires_at = entry->value.expires_at_ms;

    LWLockRelease(cache->lock);
    return true;
}

static void
l2_store(FgaL2AclCache* cache, const FgaAclCacheKey* key, TimestampTz now_ms, TimestampTz expires_at, bool allowed)
{
    bool found;
    FgaL2AclSlot* idx;
    FgaL2AclEntry* entry;

    if (cache == NULL)
        return;

    LWLockAcquire(cache->lock, LW_EXCLUSIVE);

    /* 1. 기존 엔트리 업데이트 */
    idx = hash_search(l2_slot_table, key, HASH_FIND, &found);
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
        hash_search(l2_slot_table, &entry->key, HASH_REMOVE, NULL);
    }

    /* 3. 새 엔트리 생성 */
    idx = hash_search(l2_slot_table, key, HASH_ENTER, &found);
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

#endif /* POSTFGA_CACHE_L2_ACL_H */
