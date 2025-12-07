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
#ifndef FGA_SHMEM_H
#define FGA_SHMEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

#include <storage/latch.h>
#include <storage/lwlock.h>

    // forward declaration
    struct FgaChannel;
    typedef struct FgaChannel FgaChannel;

    struct FgaL2AclCache;
    typedef struct FgaL2AclCache FgaL2AclCache;

    struct FgaStats;
    typedef struct FgaStats FgaStats;

    /*-------------------------------------------------------------------------
     * FgaState
     */
    typedef struct FgaState
    {
        LWLock* lock;         /* Master lock for all shared data */
        Latch* bgw_latch;     /* Background worker latch */
        uint64_t hash_seed;   /* Hash seed for consistent hashing */
        FgaChannel* channel;  /* Request channel */
        FgaL2AclCache* cache; /* L2 cache */
        FgaStats* stats;      /* Statistics */
    } FgaState;

    /* 전역 shmem state 포인터 (실제 정의는 shmem.c 에서) */
    extern FgaState* fga_state_instance_;

    /* Shmem lifecycle API */
    void fga_shmem_request(void);
    void fga_shmem_startup(void);

    // Accessor for global shmem state
    static inline FgaState* fga_get_state(void)
    {
        return fga_state_instance_;
    }

    static inline FgaChannel* fga_get_channel()
    {
        return fga_state_instance_->channel;
    }

    static inline FgaStats* fga_get_stats()
    {
        return fga_state_instance_->stats;
    }

    static inline void fga_wake_bgw()
    {
        SetLatch(fga_state_instance_->bgw_latch);
    }

#ifdef __cplusplus
}
#endif

#endif /* FGA_SHMEM_H */
