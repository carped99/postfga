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

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

#include <storage/latch.h>
#include <storage/lwlock.h>

#include "stats_type.h"

    // forward declaration
    struct FgaChannel;
    typedef struct FgaChannel FgaChannel;

    struct FgaL2Cache;
    typedef struct FgaL2Cache FgaL2Cache;

    /*-------------------------------------------------------------------------
     * PostfgaShmemState
     */
    typedef struct PostfgaShmemState
    {
        LWLock* lock;        /* Master lock for all shared data */
        Latch* bgw_latch;    /* Background worker latch */
        uint64_t hash_seed;  /* Hash seed for consistent hashing */
        FgaChannel* channel; /* Request channel */
        FgaL2Cache* cache;   /* L2 cache */
        FgaStats stats;      /* Statistics */

    } PostfgaShmemState;

    /* 전역 shmem state 포인터 (실제 정의는 shmem.c 에서) */
    extern PostfgaShmemState* postfga_shmem_state_instance_;

    /* Shmem lifecycle API */
    void postfga_shmem_request(void);
    void postfga_shmem_startup(void);

    // Accessor for global shmem state
    static inline PostfgaShmemState* postfga_get_shmem_state(void)
    {
        return postfga_shmem_state_instance_;
    }

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_SHMEM_H */
