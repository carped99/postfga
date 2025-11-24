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
#include <storage/lwlock.h>
#include <storage/latch.h>

#include "cache.h"
#include "stats_type.h"
#include "request_queue.h"
#include "request_slot.h"

#ifdef __cplusplus
}
#endif

/*-------------------------------------------------------------------------
 * PostfgaShmemState
 */
typedef struct PostfgaShmemState
{
    LWLock *lock;                   /* Master lock for all shared data */
    Latch *bgw_latch;               /* Background worker latch */
    uint64_t hash_seed;             /* Hash seed for consistent hashing */
    pg_atomic_uint64 request_id;    /* Request identifier */
    FgaRequestSlot *request_slot;   /* Request slot */
    FgaRequestQueue *request_queue; /* Request queue */
    FgaL2Cache l2_cache;            /* Shared L2 cache */
    FgaStats stats;                 /* Statistics */

} PostfgaShmemState;

#ifdef __cplusplus
extern "C"
{
#endif
    /* Shmem lifecycle API */
    void postfga_shmem_request(void);
    void postfga_shmem_startup(void);

    // Accessor for global shmem state
    PostfgaShmemState *postfga_get_shmem_state(void);
    FgaL2Cache *postfga_get_cache_state(void);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_SHMEM_H */
