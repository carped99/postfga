/*-------------------------------------------------------------------------
 *
 * shmem.h
 *    Shared memory lifecycle API for PostFGA extension.
 *
 * Responsibilities:
 *    - Request shared memory size before allocation
 *    - Initialize shared memory after allocation
 *    - Provide read-only access to shared state pointer
 *
 *-------------------------------------------------------------------------
 */
#ifndef POSTFGA_SHMEM_H
#define POSTFGA_SHMEM_H

#include <postgres.h>
#include <storage/lwlock.h>
#include <storage/latch.h>

#include "cache.h"
#include "stats.h"
#include "queue.h"

/*
 * PostfgaShmemState
 */
typedef struct PostfgaShmemState
{
    /* ---- Synchronization ---- */
    LWLock *lock;     /* Master lock for all shared data */

    Latch *bgw_latch; /* Background worker latch */

    FgaSlot      *slots;                     /* Request slots 배열 */
    pg_atomic_uint64 next_request_id;
    PostfgaRequestQueue request_queue;

    /* L2 캐시 */
    FgaL2Cache l2_cache;

    /* ---- Statistics ---- */
    Stats stats;

} PostfgaShmemState;

/*
 * C++에서 이 헤더를 include할 때
 * 심볼 이름이 C 방식으로 유지되도록 보장
 */
#ifdef __cplusplus
extern "C"
{
#endif
    /* 전역 shared state 포인터 조회용 */
    extern PostfgaShmemState *PostfgaSharedState;

    /* Shmem lifecycle API */
    void postfga_shmem_request(void);
    void postfga_shmem_startup(void);

    static inline PostfgaShmemState *postfga_get_sheme_state(void)
    {
        return PostfgaSharedState;
    }

    Cache *postfga_get_cache_state(void);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_SHMEM_H */
