#ifndef POSTFGA_STATE_H
#define POSTFGA_STATE_H

#include <postgres.h>
#include <storage/latch.h>
#include <storage/lwlock.h>
#include <utils/hsearch.h>

#include "cache.h"
#include "queue.h"
#include "stats.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * PostfgaShmemState
     */
    typedef struct PostfgaShmemState
    {
        /* ---- Synchronization ---- */
        LWLock *lock;     /* Master lock for all shared data */
        Latch *bgw_latch; /* Background worker latch */

        /* ---- Global generation counter ---- */
        uint64 next_generation; /* Next generation number */

        Cache cache; /* 통합 응답 캐시 구조체 */
        PostfgaRequestQueue request_queue;

        /* ---- Statistics ---- */
        Stats stats;

    } PostfgaShmemState;

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_STATE_H */