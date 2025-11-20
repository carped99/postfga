/*-------------------------------------------------------------------------
 *
 * shmem.h
 *    Shared memory state definition for PostFGA ACL cache & BGW.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTFGA_STATE_H
#define POSTFGA_STATE_H

#include <postgres.h>
#include <datatype/timestamp.h>
#include <storage/lwlock.h>
#include <storage/latch.h>
#include <storage/shmem.h>
#include <utils/hsearch.h>
#include <port/atomics.h>

#include "common.h"
#include "cache.h"
#include "stats.h"

/* Forward declarations */
typedef union RequestPayload RequestPayload;


/* Default hash table sizes */
#define DEFAULT_RELATION_COUNT    16
#define DEFAULT_CACHE_ENTRIES     10000
#define DEFAULT_GEN_MAP_SIZE      1024

/*
 * PostfgaShmemState
 */
typedef struct PostfgaShmemState
{
    /* ---- Synchronization ---- */
    LWLock              *lock;          /* Master lock for all shared data */
    Latch               *bgw_latch;     /* Background worker latch */

    /* ---- Global generation counter ---- */
    uint64              next_generation; /* Next generation number */

    /* ---- Hash tables ---- */
    HTAB                *relation_bitmap_map;    /* Relation name -> bit index */
    HTAB                *acl_cache;              /* ACL cache entries */
    HTAB                *object_type_gen_map;    /* object_type -> generation */
    HTAB                *object_gen_map;         /* object_type:object_id -> generation */
    HTAB                *subject_type_gen_map;   /* subject_type -> generation */
    HTAB                *subject_gen_map;        /* subject_type:subject_id -> generation */

    /* ---- Request queue (ring buffer) ---- */
    pg_atomic_uint32    queue_head;              /* Ring buffer head index */
    pg_atomic_uint32    queue_tail;              /* Ring buffer tail index */
    pg_atomic_uint32    queue_size;              /* Current queue size */
    RequestPayload      *request_queue;          /* Array of MAX_PENDING_REQ elements (tagged union) */

    /* ---- Statistics ---- */
    Stats               stats;

} PostfgaShmemState;

/* Request shared memory allocation (called from _PG_init) */
void postfga_shmem_request(void);

/* Initialize shared memory structures (called from shmem_startup_hook) */
void postfga_shmem_startup(void);

/* Get pointer to shared state */
PostfgaShmemState *get_shared_state(void);

#endif /* POSTFGA_STATE_H */