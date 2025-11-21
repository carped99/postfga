#ifndef POSTFGA_CACHE_H
#define POSTFGA_CACHE_H

#include <postgres.h>
#include <utils/hsearch.h>
#include <c.h>

/*
 * Cache
 *
 * - ACL 캐시 + generation 맵을 하나로 묶은 구조체
 * - shared memory 안에 그대로 들어감 (포인터만 저장)
 */
typedef struct Cache
{
    /* ---- Global generation counter ---- */
    uint64      next_generation;        /* cache invalidation용 전역 generation */

    /* ---- Hash tables ---- */
    HTAB       *relation_bitmap_map;    /* relation name -> bit index */
    HTAB       *acl_cache;              /* ACL cache entries */
    HTAB       *object_type_gen_map;    /* object_type            -> generation */
    HTAB       *object_gen_map;         /* object_type:object_id  -> generation */
    HTAB       *subject_type_gen_map;   /* subject_type           -> generation */
    HTAB       *subject_gen_map;        /* subject_type:subject_id-> generation */
} Cache;

/* 필요한 경우 헬퍼 함수들 (선택) */

static inline uint64
postfga_cache_next_generation(Cache *cache)
{
    return ++cache->next_generation;
}


Size postfga_cache_estimate_size(int max_cache_entries);

void postfga_init_cache(Cache *cache, int max_cache_entries);
#endif /* POSTFGA_CACHE_H */
