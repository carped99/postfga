#ifndef POSTFGA_CACHE_L1_H
#define POSTFGA_CACHE_L1_H

/*
 * L1 Cache (per-backend)
 * - 2-way set-associative
 * - pseudo-LRU via flip-bit (victim index 0/1)
 *
 * 총 엔트리 수: L1_NUM_SETS * L1_NUM_WAYS
 *   현재: 4096 * 2 = 8192 entries
 */

#include <postgres.h>

#include <utils/memutils.h>
#include <utils/timestamp.h> /* TimestampTz, if needed for time source */

#include "cache.h"

/*-------------------------------------------------------------------------
 * 파라미터
 *-------------------------------------------------------------------------*/

#define FGA_L1_NUM_SETS_BITS 14
#define FGA_L1_NUM_SETS (1 << FGA_L1_NUM_SETS_BITS) /* 16384 sets */
#define FGA_L1_NUM_WAYS 2                           /* 2-way */
_Static_assert((FGA_L1_NUM_SETS & (FGA_L1_NUM_SETS - 1)) == 0, "L1_NUM_SETS must be power of two");

/*-------------------------------------------------------------------------
 * 구조체 정의
 *-------------------------------------------------------------------------*/

typedef struct FgaL1Entry
{
    bool valid;
    bool allowed;
    uint16_t global_gen;
    uint64_t expires_at_ms;
    FgaAclCacheKey key;
} FgaL1Entry;

typedef struct FgaL1Set
{
    FgaL1Entry ways[FGA_L1_NUM_WAYS];
    uint8_t victim; /* 0 또는 1: 다음에 evict할 후보 way */
} FgaL1Set;

typedef struct FgaL1Cache
{
    MemoryContext ctx; /* 이 캐시 전용 메모리 컨텍스트 */
    FgaL1Set sets[FGA_L1_NUM_SETS];
} FgaL1Cache;

/* per-backend static */
static FgaL1Cache* l1_cache = NULL;

/*
 * key → set index
 */
static inline uint32_t l1_hash_to_set(const FgaAclCacheKey* key)
{
    return (uint32_t)(key->low & (FGA_L1_NUM_SETS - 1));
}

/*
 * 키 비교
 *  - 필요하면 FgaAclCacheKey 내부에 hash 필드를 넣고
 *    그걸 먼저 비교한 뒤 나머지 필드를 비교하는 식으로 최적화 가능
 */
static inline bool l1_key_equals(const FgaAclCacheKey* a, const FgaAclCacheKey* b)
{
    return a->low == b->low && a->high == b->high;
}

/*-------------------------------------------------------------------------
 * Pseudo-LRU: flip-bit 방식 (2-way 전용)
 *
 * - victim: 다음에 evict할 way 인덱스 (0 또는 1)
 * - 접근된 way는 hot → 반대편을 victim으로 설정
 *-------------------------------------------------------------------------*/

static inline void l1_plru_access(FgaL1Set* set, int way)
{
    Assert(way == 0 || way == 1);
    set->victim = (uint8_t)(way ^ 1);
}

static inline int l1_plru_victim(FgaL1Set* set)
{
    return (int)set->victim;
}

/*-------------------------------------------------------------------------
 * L1 initialize
 *-------------------------------------------------------------------------*/
static void l1_startup(void)
{
    MemoryContext ctx;
    MemoryContext old;

    if (l1_cache != NULL)
        return;

    ctx = AllocSetContextCreate(TopMemoryContext, "PostFGA L1 Cache", ALLOCSET_DEFAULT_SIZES);
    old = MemoryContextSwitchTo(ctx);

    l1_cache = (FgaL1Cache*)palloc0(sizeof(FgaL1Cache));
    l1_cache->ctx = ctx;

    /*
     * palloc0 덕분에:
     * - sets[].ways[].valid      = false
     * - sets[].victim            = 0
     * 모두 초기화되어 있음.
     */

    MemoryContextSwitchTo(old);
}

/*-------------------------------------------------------------------------
 * Lookup
 *
 * - key, cur_generation, now_ms가 주어졌을 때 hit 여부와 allowed 반환
 *-------------------------------------------------------------------------*/

static bool l1_lookup(const FgaAclCacheKey* const key, uint16_t cur_generation, TimestampTz now_ms, bool* allowed_out)
{
    FgaL1Set* set;

    if (l1_cache == NULL)
        return false;

    set = &l1_cache->sets[l1_hash_to_set(key)];

    for (int i = 0; i < FGA_L1_NUM_WAYS; i++)
    {
        FgaL1Entry* e = &set->ways[i];

        if (!e->valid)
            continue;

        if (!l1_key_equals(&e->key, key))
            continue;

        /* TTL 만료 */
        if (e->expires_at_ms <= now_ms)
        {
            e->valid = false;
            return false;
        }

        /* generation mismatch → lazy invalidation */
        if (e->global_gen != cur_generation)
        {
            e->valid = false;
            return false;
        }

        /* hit */
        l1_plru_access(set, i);
        *allowed_out = e->allowed;

        return true;
    }
    return false;
}

/*-------------------------------------------------------------------------
 * Store
 *
 * - key / generation / expires_at / allowed 값을 L1에 넣거나 갱신
 *-------------------------------------------------------------------------*/

static void l1_store(const FgaAclCacheKey* key, uint16_t generation, TimestampTz expires_at_ms, bool allowed)
{
    uint32_t set_idx;
    FgaL1Set* set;
    FgaL1Entry* e;
    int empty_slot = -1;

    if (l1_cache == NULL)
        return;

    set_idx = l1_hash_to_set(key);
    set = &l1_cache->sets[set_idx];


    for (int i = 0; i < FGA_L1_NUM_WAYS; i++)
    {
        e = &set->ways[i];

        if (!e->valid)
        {
            if (empty_slot < 0)
                empty_slot = i;
            continue;
        }

        if (l1_key_equals(&e->key, key))
        {
            /* 1) 이미 존재하는 key → update */
            e->allowed = allowed;
            e->expires_at_ms = expires_at_ms;
            e->global_gen = generation;

            l1_plru_access(set, i);
            return;
        }
    }

    /* 빈 슬롯 사용 또는 victim 교체 */
    int target = (empty_slot >= 0) ? empty_slot : l1_plru_victim(set);
    e = &set->ways[target];

    e->valid = true;
    e->key = *key;
    e->allowed = allowed;
    e->expires_at_ms = expires_at_ms;
    e->global_gen = generation;
    l1_plru_access(set, target);
}

/*-------------------------------------------------------------------------
 * 전체 무효화
 *-------------------------------------------------------------------------*/

static void l1_invalidate_all(void)
{
    if (l1_cache == NULL)
        return;

    for (int s = 0; s < FGA_L1_NUM_SETS; s++)
    {
        FgaL1Set* set = &l1_cache->sets[s];

        for (int w = 0; w < FGA_L1_NUM_WAYS; w++)
            set->ways[w].valid = false;

        set->victim = 0;
    }

    elog(DEBUG1, "L1 cache invalidated (all)");
}

/*-------------------------------------------------------------------------
 * 특정 generation 기반 무효화 (eager)
 *  - lazy 방식(lookup 시 generation mismatch 처리) 대신
 *    한 번에 쓸어버리고 싶을 때 사용
 *-------------------------------------------------------------------------*/

static void l1_invalidate_by_generation(uint16_t old_generation)
{
    if (l1_cache == NULL)
        return;

    for (int s = 0; s < FGA_L1_NUM_SETS; s++)
    {
        FgaL1Set* set = &l1_cache->sets[s];

        for (int w = 0; w < FGA_L1_NUM_WAYS; w++)
        {
            FgaL1Entry* e = &set->ways[w];

            if (e->valid && e->global_gen == old_generation)
                e->valid = false;
        }
    }

    elog(DEBUG1, "L1 cache invalidated by generation=%u", old_generation);
}

#endif /* POSTFGA_CACHE_L1_H */