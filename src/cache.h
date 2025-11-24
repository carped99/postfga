#ifndef POSTFGA_CACHE_H
#define POSTFGA_CACHE_H

#include <postgres.h>
#include <utils/hsearch.h>
#include <utils/timestamp.h>
#include <storage/lwlock.h>
#include <c.h>

#include "postfga.h"
#include "fga_type.h"

/* ACL Cache Entry */
typedef struct FgaCacheEntry
{
    FgaCacheKey key;

    bool allowed;
    uint16_t generation; /* generation mismatch 시 invalid */
    uint16_t pad;

    uint64_t expires_at_ms; /* TTL 기준 만료 시간 (epoch ms) */
    // TimestampTz last_updated;
    // TimestampTz expire_at;
} FgaCacheEntry;

typedef struct FgaL1Cache
{
    HTAB *ht;            /* key: FgaCheckKey, entry: FgaCacheEntry */
    MemoryContext ctx;   /* 캐시 엔트리용 컨텍스트 */
    uint16_t generation; /* L2 generation을 미러링할 때 사용 */
} FgaL1Cache;

typedef struct FgaL2Cache
{
    HTAB *ht;            /* shared hash table (shmem) */
    LWLock *lock;        /* shared cache 락 */
    uint16_t generation; /* global generation */
} FgaL2Cache;

/*
 * Cache
 *
 * - ACL 캐시 + generation 맵을 하나로 묶은 구조체
 * - shared memory 안에 그대로 들어감 (포인터만 저장)
 */
typedef struct Cache
{
    LWLock *lock; /* Cache 전용 잠금 (PostfgaShmemState의 lock과 별도) */
    /* ---- Global generation counter ---- */
    uint64 next_generation; /* cache invalidation용 전역 generation */

    /* ---- Hash tables ---- */
    HTAB *relation_bitmap_map;  /* relation name -> bit index */
    HTAB *acl_cache;            /* ACL cache entries */
    HTAB *object_type_gen_map;  /* object_type            -> generation */
    HTAB *object_gen_map;       /* object_type:object_id  -> generation */
    HTAB *subject_type_gen_map; /* subject_type           -> generation */
    HTAB *subject_gen_map;      /* subject_type:subject_id-> generation */
} Cache;

/* -------------------------------------------------------------------------
 * Cache API
 * ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C"
{
#endif

    FgaCacheKey postfga_make_check_key(const FgaCheckTupleRequest *req);

    /* L1 Cache API */
    void postfga_l1_init(FgaL1Cache *cache, MemoryContext parent_ctx, long size_hint);
    bool postfga_l1_lookup(FgaL1Cache *cache,
                           const FgaCacheKey *key,
                           uint16_t cur_generation,
                           uint64_t now_ms,
                           bool *allowed_out);
    void postfga_l1_store(FgaL1Cache *cache,
                          const FgaCacheKey *key,
                          uint16_t generation,
                          uint64_t now_ms,
                          uint64_t ttl_ms,
                          bool allowed);

    /* L2 Cache API */
    void postfga_l2_init(FgaL2Cache *cache, long size_hint, LWLock *lock);
    bool postfga_l2_lookup(FgaL2Cache *cache,
                           const FgaCacheKey *key,
                           uint16_t cur_generation,
                           uint64_t now_ms,
                           bool *allowed_out);
    void postfga_l2_store(FgaL2Cache *cache,
                          const FgaCacheKey *key,
                          uint16_t generation,
                          uint64_t now_ms,
                          uint64_t ttl_ms,
                          bool allowed);

    /* L2 shared hash table 에 필요한 shmem 크기 추정 */
    Size postfga_l2_estimate_shmem_size(long size_hint);

    /* generation bump (invalidation) */
    void postfga_l2_bump_generation(FgaL2Cache *cache);
#ifdef __cplusplus
} /* extern "C" */
#endif

/* 필요한 경우 헬퍼 함수들 (선택) */

static inline uint64
postfga_cache_next_generation(Cache *cache)
{
    return ++cache->next_generation;
}

Size postfga_cache_estimate_size(int max_cache_entries);

void postfga_init_cache(Cache *cache, int max_cache_entries);
#endif /* POSTFGA_CACHE_H */
