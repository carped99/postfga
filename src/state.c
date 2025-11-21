/*-------------------------------------------------------------------------
 *
 * shmem.c
 *    Shared memory lifecycle management for PostFGA extension.
 *
 * This module implements:
 *   - Shared memory allocation and initialization
 *   - Hash table creation and setup
 *   - Atomic counter initialization
 *   - Global state management
 *
 *-------------------------------------------------------------------------
 */
 
#include <postgres.h>
#include <string.h>
#include <miscadmin.h>
#include <storage/ipc.h>
#include <storage/lwlock.h>
#include <utils/hsearch.h>

#include "cache.h"
#include "state.h"
#include "relation.h"
#include "queue.h"

/* -------------------------------------------------------------------------
 * Module-level state
 * -------------------------------------------------------------------------
 */

/* Global pointer to shared state (set once during startup) */
static PostfgaShmemState *shared_state = NULL;

/* GUC variables (defaults) */
static int max_cache_entries = DEFAULT_CACHE_ENTRIES;

/* -------------------------------------------------------------------------
 * Forward declarations (private functions)
 * -------------------------------------------------------------------------
 */

static Size calculate_shared_memory_size(void);
static void initialize_shared_state(bool found);
static void initialize_hash_table(HTAB **htab, const char *name,
                                 int nelem, Size keysize, Size entrysize);
static void initialize_atomic_counters(void);

/* -------------------------------------------------------------------------
 * Lifecycle Functions
 * -------------------------------------------------------------------------
 */

/*
 * shmem_request
 *
 * Request shared memory allocation for the extension.
 * Called from _PG_init before PostgreSQL allocates shared memory.
 *
 * This function:
 *   1. Calculates total shared memory needed
 *   2. Requests allocation via RequestAddinShmemSpace
 *   3. Requests a named LWLock tranche for synchronization
 */
void
postfga_shmem_request(void)
{
    Size shmem_size;

    shmem_size = calculate_shared_memory_size();

    RequestAddinShmemSpace(shmem_size);
    RequestNamedLWLockTranche("postfga", 1);

    elog(DEBUG1, "PostFGA: Requested %zu bytes of shared memory", shmem_size);
}

/*
 * shmem_startup
 *
 * Initialize shared memory structures.
 * Called from shmem_startup_hook after PostgreSQL allocates shared memory.
 *
 * This function:
 *   1. Acquires AddinShmemInitLock
 *   2. Creates or attaches to shared memory segment
 *   3. Initializes all hash tables and counters
 *   4. Releases the lock
 *
 * Thread safety: Uses AddinShmemInitLock to ensure single initialization.
 */
void
postfga_shmem_startup(void)
{
    bool found;

    elog(DEBUG1, "PostFGA: Initializing shared memory");

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    /* Create or attach to shared memory segment */
    shared_state = ShmemInitStruct("PostFGA Shared State",
                                   sizeof(PostfgaShmemState),
                                   &found);

    if (!found)
    {
        /* First time initialization */
        initialize_shared_state(found);
        elog(LOG, "PostFGA: Shared memory initialized successfully");
    }
    else
    {
        elog(DEBUG1, "PostFGA: Attached to existing shared memory segment");
    }

    LWLockRelease(AddinShmemInitLock);
}

/*
 * get_shared_state
 *
 * Get pointer to shared state structure.
 *
 * Returns: Pointer to shared state, or NULL if not initialized.
 *
 * Note: Caller should check for NULL before dereferencing.
 */
PostfgaShmemState *
get_shared_state(void)
{
    return shared_state;
}

/* -------------------------------------------------------------------------
 * Private Helper Functions
 * -------------------------------------------------------------------------
 */

/*
 * calculate_shared_memory_size
 *
 * Calculate total shared memory size needed for the extension.
 *
 * Returns: Total size in bytes
 */
static Size
calculate_shared_memory_size(void)
{
    Size size = 0;

    /* Main shared state structure */
    size = add_size(size, sizeof(PostfgaShmemState));

    /* Relation bitmap hash table */
    size = add_size(size, hash_estimate_size(DEFAULT_RELATION_COUNT,
                                             sizeof(RelationBitMapEntry)));

    /* ACL cache hash table */
    size = add_size(size, hash_estimate_size(max_cache_entries,
                                             sizeof(AclCacheEntry)));

    /* Generation tracking hash tables (4 maps) */
    size = add_size(size, hash_estimate_size(DEFAULT_GEN_MAP_SIZE,
                                             sizeof(GenerationEntry)));
    size = add_size(size, hash_estimate_size(max_cache_entries,
                                             sizeof(GenerationEntry)));
    size = add_size(size, hash_estimate_size(DEFAULT_GEN_MAP_SIZE,
                                             sizeof(GenerationEntry)));
    size = add_size(size, hash_estimate_size(max_cache_entries,
                                             sizeof(GenerationEntry)));

    /* Request queue (tagged union for multiple request types) */
    size = add_size(size, mul_size(MAX_PENDING_REQ, sizeof(RequestPayload)));

    return size;
}

/*
 * initialize_shared_state
 *
 * Initialize shared state structure and all hash tables.
 *
 * Args:
 *   found - true if shared memory segment already existed
 */
static void
initialize_shared_state(bool found)
{
    if (found)
    {
        /* Should not happen, but handle gracefully */
        elog(WARNING, "PostFGA: Shared state already initialized");
        return;
    }

    /* Zero out the entire structure */
    memset(shared_state, 0, sizeof(PostfgaShmemState));

    /* Initialize LWLock */
    shared_state->lock = &(GetNamedLWLockTranche("postfga")->lock);

    /* Initialize generation counter */
    shared_state->next_generation = 1;

    /* Initialize hash tables */
    initialize_hash_table(&shared_state->relation_bitmap_map,
                         "PostFGA Relation Bitmap",
                         DEFAULT_RELATION_COUNT,
                         RELATION_MAX_LEN,
                         sizeof(RelationBitMapEntry));

    initialize_hash_table(&shared_state->acl_cache,
                         "PostFGA ACL Cache",
                         max_cache_entries,
                         sizeof(AclCacheKey),
                         sizeof(AclCacheEntry));

    initialize_hash_table(&shared_state->object_type_gen_map,
                         "PostFGA Object Type Gen",
                         DEFAULT_GEN_MAP_SIZE,
                         NAME_MAX_LEN * 2,
                         sizeof(GenerationEntry));

    initialize_hash_table(&shared_state->object_gen_map,
                         "PostFGA Object Gen",
                         max_cache_entries,
                         NAME_MAX_LEN * 2,
                         sizeof(GenerationEntry));

    initialize_hash_table(&shared_state->subject_type_gen_map,
                         "PostFGA Subject Type Gen",
                         DEFAULT_GEN_MAP_SIZE,
                         NAME_MAX_LEN * 2,
                         sizeof(GenerationEntry));

    initialize_hash_table(&shared_state->subject_gen_map,
                         "PostFGA Subject Gen",
                         max_cache_entries,
                         NAME_MAX_LEN * 2,
                         sizeof(GenerationEntry));

    /* Initialize request queue (tagged union) */
    shared_state->request_queue = (RequestPayload *)ShmemAlloc(
        mul_size(MAX_PENDING_REQ, sizeof(RequestPayload)));

    if (shared_state->request_queue == NULL)
        elog(ERROR, "PostFGA: Failed to allocate request queue");

    memset(shared_state->request_queue, 0, mul_size(MAX_PENDING_REQ, sizeof(RequestPayload)));

    /* Initialize atomic counters */
    initialize_atomic_counters();

    elog(DEBUG1, "PostFGA: Shared state initialization complete");
}

/*
 * initialize_hash_table
 *
 * Initialize a shared memory hash table.
 *
 * Args:
 *   htab      - Pointer to HTAB pointer (output)
 *   name      - Name of the hash table (for debugging)
 *   nelem     - Expected number of elements
 *   keysize   - Size of hash key
 *   entrysize - Size of hash entry
 */
static void
initialize_hash_table(HTAB **htab, const char *name,
                     int nelem, Size keysize, Size entrysize)
{
    HASHCTL hash_ctl;

    memset(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = keysize;
    hash_ctl.entrysize = entrysize;

    *htab = ShmemInitHash(name,
                         nelem,
                         nelem,
                         &hash_ctl,
                         HASH_ELEM | HASH_BLOBS);

    if (*htab == NULL)
        elog(ERROR, "PostFGA: Failed to create hash table '%s'", name);

    elog(DEBUG2, "PostFGA: Created hash table '%s' with %d elements", name, nelem);
}

/*
 * initialize_atomic_counters
 *
 * Initialize all atomic counters to zero.
 */
static void
initialize_atomic_counters(void)
{
    /* Initialize queue atomic counters */
    pg_atomic_init_u32(&shared_state->queue_head, 0);
    pg_atomic_init_u32(&shared_state->queue_tail, 0);
    pg_atomic_init_u32(&shared_state->queue_size, 0);

    /* Initialize statistics */
    stats_init(&shared_state->stats);

    elog(DEBUG2, "PostFGA: Atomic counters initialized");
}
