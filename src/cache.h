#ifndef POSTFGA_CACHE_H
#define POSTFGA_CACHE_H

#include <postgres.h>

#include <c.h>
#include <storage/lwlock.h>
#include <utils/hsearch.h>
#include <utils/timestamp.h>

#include "payload.h"
#include "postfga.h"

typedef struct FgaRelationCacheEntry
{
    char name[RELATION_MAX_LEN];
    uint16_t id;
} FgaRelationCacheEntry;


typedef struct FgaAclCacheKey
{
    uint64_t low;
    uint64_t high;
    uint16_t relation_id;
    uint16_t pad;
    uint64_t object_key;
} FgaAclCacheKey;

typedef struct FgaAclCacheValue
{
    bool allowed;

    uint16_t global_gen; /* generation mismatch 시 invalid */
    uint16_t object_gen; /* generation mismatch 시 invalid */

    uint64_t expires_at_ms; /* TTL 기준 만료 시간 (epoch ms) */
} FgaAclCacheValue;

/* ACL Cache Entry */
typedef struct FgaAclCacheEntry
{
    FgaAclCacheKey key;
    FgaAclCacheValue value;
} FgaAclCacheEntry;

typedef struct FgaL1Cache
{
    MemoryContext ctx;   /* 캐시 엔트리용 컨텍스트 */
    uint16_t generation; /* L2 generation을 미러링할 때 사용 */
    HTAB* acl;           /* key: FgaCheckKey, entry: FgaCacheEntry */
    HTAB* relation;      /* relation name -> bit index */
} FgaL1Cache;

typedef struct FgaL2Cache
{
    LWLock* lock;        /* shared cache 락 */
    uint16_t generation; /* global generation */
    HTAB* acl;           /* shared hash table (shmem) */
    // HTAB* relation;      /* relation name -> bit index */
} FgaL2Cache;

/*
 * Cache
 *
 * - ACL 캐시 + generation 맵을 하나로 묶은 구조체
 * - shared memory 안에 그대로 들어감 (포인터만 저장)
 */
// typedef struct Cache
// {
//     LWLock *lock; /* Cache 전용 잠금 (PostfgaShmemState의 lock과 별도) */
//     /* ---- Global generation counter ---- */
//     uint64 next_generation; /* cache invalidation용 전역 generation */

//     /* ---- Hash tables ---- */
//     HTAB *relation_bitmap_map;  /* relation name -> bit index */
//     HTAB *acl_cache;            /* ACL cache entries */
//     HTAB *object_type_gen_map;  /* object_type            -> generation */
//     HTAB *object_gen_map;       /* object_type:object_id  -> generation */
//     HTAB *subject_type_gen_map; /* subject_type           -> generation */
//     HTAB *subject_gen_map;      /* subject_type:subject_id-> generation */
// } Cache;

/* -------------------------------------------------------------------------
 * Cache API
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif
    /* generation bump (invalidation) */
    void postfga_l2_bump_generation(FgaL2Cache* cache);


    void postfga_cache_key(FgaAclCacheKey* key,
                           const char* store_id,
                           const char* model_id,
                           text* object_type,
                           text* object_id,
                           text* subject_type,
                           text* subject_id,
                           text* relation);

    bool postfga_cache_lookup(const FgaAclCacheKey* key, uint64_t ttl_ms, bool* allowed_out);

    void postfga_cache_store(const FgaAclCacheKey* key, uint64_t ttl_ms, bool allowed);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* POSTFGA_CACHE_H */
