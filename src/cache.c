#include <postgres.h>
#include <utils/hsearch.h>
#include <utils/memutils.h>
#include <storage/lwlock.h>
#include <storage/shmem.h>

#include <xxhash.h>

#include "postfga.h"
#include "cache.h"
#include "relation.h"
#include "generation.h"


FgaCacheKey
postfga_make_check_key(const FgaCheckTupleRequest *req)
{
    FgaCacheKey key;

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
    if (len > (int) sizeof(buf))
        len = sizeof(buf);

    /* Postgres hash_any 사용 (uint32 반환) → 두 번 돌려서 64bit 구성 */
    key.hash  = (uint64_t) XXH64(buf, (size_t) len, 0);
    key.hash2 = (uint64_t) XXH64(buf, (size_t) len, 1);    

    return key;
}

typedef struct FgaL1HashEntry
{
    FgaCacheKey   key;
    FgaCacheEntry entry;
} FgaL1HashEntry;

void
postfga_l1_init(FgaL1Cache *cache, MemoryContext parent_ctx, long size_hint)
{
    HASHCTL ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaCacheKey);
    ctl.entrysize = sizeof(FgaL1HashEntry);
    ctl.hcxt = AllocSetContextCreate(parent_ctx,
                                     "PostFGA L1 Cache",
                                     ALLOCSET_SMALL_SIZES);

    cache->ht = hash_create("PostFGA L1 Cache",
                            size_hint > 0 ? size_hint : 1024,
                            &ctl,
                            HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

    cache->ctx = ctl.hcxt;
    cache->generation = 0;
}

bool
postfga_l1_lookup(FgaL1Cache *cache,
                  const FgaCacheKey *key,
                  uint16_t cur_generation,
                  uint64_t now_ms,
                  bool *allowed_out)
{
    bool found;
    FgaCacheEntry *ce;
    FgaL1HashEntry *he;

    if (cache->ht == NULL)
        return false;

    he = (FgaL1HashEntry *) hash_search(cache->ht,
                                        (void *) key,
                                        HASH_FIND,
                                        &found);
    if (!found)
        return false;

    ce = &he->entry;

    /* generation mismatch → stale */
    if (ce->generation != cur_generation)
        return false;

    /* TTL 만료 */
    if (ce->expires_at_ms != 0 && ce->expires_at_ms < now_ms)
        return false;

    *allowed_out = ce->allowed;
    return true;
}

void
postfga_l1_store(FgaL1Cache *cache,
                 const FgaCacheKey *key,
                 uint16_t generation,
                 uint64_t now_ms,
                 uint64_t ttl_ms,
                 bool allowed)
{
    bool found;
    FgaL1HashEntry *he;

    if (cache->ht == NULL)
        return;

    he = (FgaL1HashEntry *) hash_search(cache->ht,
                                        (void *) key,
                                        HASH_ENTER,
                                        &found);

    he->key = *key;
    he->entry.key = *key;
    he->entry.allowed = allowed;
    he->entry.generation = generation;
    he->entry.expires_at_ms = (ttl_ms > 0) ? (now_ms + ttl_ms) : 0;
}

typedef struct FgaL2HashEntry
{
    FgaCacheKey   key;
    FgaCacheEntry entry;
} FgaL2HashEntry;

void
postfga_l2_init(FgaL2Cache *cache, long size_hint, LWLock *lock)
{
    HASHCTL ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaCacheKey);
    ctl.entrysize = sizeof(FgaL2HashEntry);
    ctl.hcxt = ShmemInitStruct("PostFGA L2 Cache Context",
                               0,
                               NULL);

    cache->ht = ShmemInitHash("PostFGA L2 Cache",
                              size_hint > 0 ? size_hint : 16384,
                              size_hint > 0 ? size_hint : 16384,
                              &ctl,
                              HASH_ELEM | HASH_BLOBS | HASH_CONTEXT | HASH_SHARED_MEM);

    cache->lock = lock;
    cache->generation = 0;
}

bool
postfga_l2_lookup(FgaL2Cache *cache,
                  const FgaCacheKey *key,
                  uint16_t cur_generation,
                  uint64_t now_ms,
                  bool *allowed_out)
{
    bool found;
    FgaCacheEntry *ce;
    FgaL2HashEntry *he;

    if (cache->ht == NULL)
        return false;

    LWLockAcquire(cache->lock, LW_SHARED);

    he = (FgaL2HashEntry *) hash_search(cache->ht,
                                        (void *) key,
                                        HASH_FIND,
                                        &found);
    if (!found)
    {
        LWLockRelease(cache->lock);
        return false;
    }

    ce = &he->entry;

    /* generation mismatch */
    if (ce->generation != cur_generation)
    {
        LWLockRelease(cache->lock);
        return false;
    }

    /* TTL 만료 */
    if (ce->expires_at_ms != 0 && ce->expires_at_ms < now_ms)
    {
        LWLockRelease(cache->lock);
        return false;
    }

    *allowed_out = ce->allowed;

    LWLockRelease(cache->lock);
    return true;
}

void
postfga_l2_store(FgaL2Cache *cache,
                 const FgaCacheKey *key,
                 uint16_t generation,
                 uint64_t now_ms,
                 uint64_t ttl_ms,
                 bool allowed)
{
    bool found;
    FgaL2HashEntry *he;

    if (cache->ht == NULL)
        return;

    LWLockAcquire(cache->lock, LW_EXCLUSIVE);

    he = (FgaL2HashEntry *) hash_search(cache->ht,
                                        (void *) key,
                                        HASH_ENTER,
                                        &found);

    he->key = *key;
    he->entry.key = *key;
    he->entry.allowed = allowed;
    he->entry.generation = generation;
    he->entry.expires_at_ms = (ttl_ms > 0) ? (now_ms + ttl_ms) : 0;

    LWLockRelease(cache->lock);
}

Size
postfga_l2_estimate_shmem_size(long size_hint)
{
    long n = (size_hint > 0) ? size_hint : 16384;

    /* FgaL2HashEntry 는 L2 해시에서 쓰는 entry 구조체 */
    return EstimateSharedHashTableSize(n, sizeof(FgaL2HashEntry));
}

Size postfga_cache_estimate_size(int max_cache_entries)
{
    Size size = 0;

    // /* relation bitmap map */
    // size = add_size(size,
    //                 hash_estimate_size(DEFAULT_RELATION_COUNT,
    //                                    sizeof(RelationBitMapEntry)));

    // /* ACL cache */
    // size = add_size(size,
    //                 hash_estimate_size(max_cache_entries,
    //                                    sizeof(AclCacheEntry)));

    // /* Generation maps (object/subject 타입 & 값) */
    // size = add_size(size,
    //                 hash_estimate_size(DEFAULT_GEN_MAP_SIZE,
    //                                    sizeof(GenerationEntry))); /* object_type */
    // size = add_size(size,
    //                 hash_estimate_size(max_cache_entries,
    //                                    sizeof(GenerationEntry))); /* object */
    // size = add_size(size,
    //                 hash_estimate_size(DEFAULT_GEN_MAP_SIZE,
    //                                    sizeof(GenerationEntry))); /* subject_type */
    // size = add_size(size,
    //                 hash_estimate_size(max_cache_entries,
    //                                    sizeof(GenerationEntry))); /* subject */

    return size;
}

void postfga_init_cache(Cache *cache, int max_cache_entries)
{
    Assert(cache != NULL);

    memset(cache, 0, sizeof(Cache));

//     /* Relation bitmap hash */
//     init_hash(&cache->relation_bitmap_map,
//               "PostFGA Relation Bitmap",
//               DEFAULT_RELATION_COUNT,
//               RELATION_MAX_LEN,
//               sizeof(RelationBitMapEntry));

//     /* ACL cache */
//     init_hash(&cache->acl_cache,
//               "PostFGA ACL Cache",
//               max_cache_entries,
//               sizeof(AclCacheKey),
//               sizeof(AclCacheEntry));

//     /* Generation maps */
//     init_hash(&cache->object_type_gen_map,
//               "PostFGA Object Type Gen",
//               DEFAULT_GEN_MAP_SIZE,
//               NAME_MAX_LEN * 2,
//               sizeof(GenerationEntry));

//     init_hash(&cache->object_gen_map,
//               "PostFGA Object Gen",
//               max_cache_entries,
//               NAME_MAX_LEN * 2,
//               sizeof(GenerationEntry));

//     init_hash(&cache->subject_type_gen_map,
//               "PostFGA Subject Type Gen",
//               DEFAULT_GEN_MAP_SIZE,
//               NAME_MAX_LEN * 2,
//               sizeof(GenerationEntry));

//     init_hash(&cache->subject_gen_map,
//               "PostFGA Subject Gen",
//               max_cache_entries,
//               NAME_MAX_LEN * 2,
//               sizeof(GenerationEntry));
}
