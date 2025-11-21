/*-------------------------------------------------------------------------
 *
 * postfga_shmem.c
 *    Shared memory lifecycle management for PostFGA extension.
 *
 * Responsibilities:
 *   - Shared memory size estimation
 *   - Global shared state allocation and initialization
 *   - Shared hash table creation and setup
 *   - Atomic counters and queue initialization
 *
 *-------------------------------------------------------------------------
 */

#include <postgres.h>

// #include "miscadmin.h"
#include <storage/ipc.h>
#include "storage/lwlock.h"
#include <storage/shmem.h>
#include <utils/hsearch.h>


#include "common.h"
#include "shmem.h"
#include "generation.h"
#include "relation.h"
#include "queue.h"
#include "cache.h"

/*-------------------------------------------------------------------------
 * Module-level state
 *-------------------------------------------------------------------------*/

/*
 * Global pointer to shared state.
 * - Set exactly once in postfga_shmem_startup()
 * - Read via postfga_get_shared_state()
 */
static PostfgaShmemState *shared_state = NULL;

/*
 * GUC-backed configuration.
 *
 * max_cache_entries 값은 다른 모듈에서 GUC로 설정될 수 있고,
 * 여기서는 "현재 설정된 값"을 사용해 shared memory 크기를 계산한다.
 */
static int max_cache_entries = DEFAULT_CACHE_ENTRIES;

/*-------------------------------------------------------------------------
 * Static helpers (private)
 *-------------------------------------------------------------------------*/

static Size postfga_shmem_calculate_size(void);
static void postfga_shmem_initialize_state(bool found);
static void postfga_shmem_init_hash(HTAB      **htab,
                                    const char *name,
                                    int         nelem,
                                    Size        keysize,
                                    Size        entrysize);
static void postfga_shmem_init_atomic_counters(void);

/*-------------------------------------------------------------------------
 * Public lifecycle API
 *-------------------------------------------------------------------------*/

/*
 * postfga_shmem_request
 *
 * Called from _PG_init() before PostgreSQL allocates shared memory.
 *
 * - Estimates total shared memory size for this extension
 * - Requests allocation via RequestAddinShmemSpace()
 * - Registers a named LWLock tranche for synchronization
 */
void
postfga_shmem_request(void)
{
    Size shmem_size = postfga_shmem_calculate_size();

    RequestAddinShmemSpace(shmem_size);
    RequestNamedLWLockTranche("postfga", 1);

    elog(DEBUG1,
         "PostFGA shmem: requested %zu bytes (max_cache_entries=%d)",
         shmem_size,
         max_cache_entries);
}

/*
 * postfga_shmem_startup
 *
 * Called from shmem_startup_hook after PostgreSQL allocates shared memory.
 *
 * - Uses AddinShmemInitLock to ensure single initialization
 * - Creates or attaches to shared memory segment
 * - Initializes all hash tables, counters, and queues
 */
void
postfga_shmem_startup(void)
{
    bool found = false;

    elog(DEBUG1, "PostFGA shmem: startup");

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    shared_state = ShmemInitStruct("PostFGA Shared State",
                                   sizeof(PostfgaShmemState),
                                   &found);

    if (!shared_state)
    {
        LWLockRelease(AddinShmemInitLock);
        elog(ERROR, "PostFGA shmem: ShmemInitStruct returned NULL");
    }

    if (!found)
    {
        /* First-time initialization */
        postfga_shmem_initialize_state(found);
        elog(LOG, "PostFGA shmem: initialized new shared state");
    }
    else
    {
        elog(DEBUG1, "PostFGA shmem: attached to existing shared state");
    }

    LWLockRelease(AddinShmemInitLock);
}

/*
 * postfga_get_shared_state
 *
 * Read-only access to global shared_state pointer.
 *
 * Caller must:
 *   - Check for NULL (e.g., called too early)
 *   - Ensure proper locking when accessing shared_state fields
 */
PostfgaShmemState *
postfga_get_shared_state(void)
{
    return shared_state;
}

/*-------------------------------------------------------------------------
 * Static helper implementations
 *-------------------------------------------------------------------------*/

/*
 * postfga_shmem_calculate_size
 *
 * Estimate the total shared memory size required for PostFGA.
 */
static Size
postfga_shmem_calculate_size(void)
{
    Size size = 0;

    /* Main shared state structure */
    size = add_size(size, sizeof(PostfgaShmemState));

    /* Relation bitmap hash table */
    size = add_size(size,
                    hash_estimate_size(DEFAULT_RELATION_COUNT,
                                       sizeof(RelationBitMapEntry)));

    /* ACL cache hash table */
    size = add_size(size,
                    hash_estimate_size(max_cache_entries,
                                       sizeof(AclCacheEntry)));

    /* Generation tracking hash tables (4 maps) */
    size = add_size(size,
                    hash_estimate_size(DEFAULT_GEN_MAP_SIZE,
                                       sizeof(GenerationEntry)));
    size = add_size(size,
                    hash_estimate_size(max_cache_entries,
                                       sizeof(GenerationEntry)));
    size = add_size(size,
                    hash_estimate_size(DEFAULT_GEN_MAP_SIZE,
                                       sizeof(GenerationEntry)));
    size = add_size(size,
                    hash_estimate_size(max_cache_entries,
                                       sizeof(GenerationEntry)));

    /* Request queue (tagged union for multiple request types) */
    size = add_size(size,
                    mul_size(MAX_PENDING_REQ,
                             sizeof(RequestPayload)));

    return size;
}

/*
 * postfga_shmem_initialize_state
 *
 * Zero out the shared_state structure, set up locks,
 * create hash tables, initialize queue and counters.
 */
static void
postfga_shmem_initialize_state(bool found)
{
    if (found)
    {
        /*
         * Defensive programming: this 상황은 정상적으론 오지 않지만,
         * 혹시 모를 잘못된 호출에도 크래시 대신 경고만 남기고 리턴.
         */
        elog(WARNING,
             "PostFGA shmem: initialize_state called with found=true");
        return;
    }

    memset(shared_state, 0, sizeof(PostfgaShmemState));

    /* Initialize LWLock from named tranche */
    shared_state->lock = &(GetNamedLWLockTranche("postfga")->lock);

    /* Generation counter starts from 1 (0은 'uninitialized'로 예약 가능) */
    shared_state->next_generation = 1;

    /* Relation bitmap hash */
    postfga_shmem_init_hash(&shared_state->relation_bitmap_map,
                            "PostFGA Relation Bitmap",
                            DEFAULT_RELATION_COUNT,
                            RELATION_MAX_LEN,
                            sizeof(RelationBitMapEntry));

    /* ACL cache hash */
    postfga_shmem_init_hash(&shared_state->acl_cache,
                            "PostFGA ACL Cache",
                            max_cache_entries,
                            sizeof(AclCacheKey),
                            sizeof(AclCacheEntry));

    /* Generation maps */
    postfga_shmem_init_hash(&shared_state->object_type_gen_map,
                            "PostFGA Object Type Gen",
                            DEFAULT_GEN_MAP_SIZE,
                            NAME_MAX_LEN * 2,
                            sizeof(GenerationEntry));

    postfga_shmem_init_hash(&shared_state->object_gen_map,
                            "PostFGA Object Gen",
                            max_cache_entries,
                            NAME_MAX_LEN * 2,
                            sizeof(GenerationEntry));

    postfga_shmem_init_hash(&shared_state->subject_type_gen_map,
                            "PostFGA Subject Type Gen",
                            DEFAULT_GEN_MAP_SIZE,
                            NAME_MAX_LEN * 2,
                            sizeof(GenerationEntry));

    postfga_shmem_init_hash(&shared_state->subject_gen_map,
                            "PostFGA Subject Gen",
                            max_cache_entries,
                            NAME_MAX_LEN * 2,
                            sizeof(GenerationEntry));

    /* Request queue allocation */
    shared_state->request_queue =
        (RequestPayload *) ShmemAlloc(
            mul_size(MAX_PENDING_REQ, sizeof(RequestPayload)));

    if (shared_state->request_queue == NULL)
        elog(ERROR, "PostFGA shmem: failed to allocate request queue");

    memset(shared_state->request_queue,
           0,
           mul_size(MAX_PENDING_REQ, sizeof(RequestPayload)));

    /* Atomic counters & stats */
    postfga_shmem_init_atomic_counters();

    elog(DEBUG1, "PostFGA shmem: shared_state initialization complete");
}

/*
 * postfga_shmem_init_hash
 *
 * Create a shared memory hash table with the given parameters.
 */
static void
postfga_shmem_init_hash(HTAB       **htab,
                        const char  *name,
                        int          nelem,
                        Size         keysize,
                        Size         entrysize)
{
    HASHCTL hash_ctl;

    memset(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize   = keysize;
    hash_ctl.entrysize = entrysize;

    *htab = ShmemInitHash(name,
                          nelem,
                          nelem,
                          &hash_ctl,
                          HASH_ELEM | HASH_BLOBS);

    if (*htab == NULL)
        elog(ERROR, "PostFGA shmem: failed to create hash '%s'", name);

    elog(DEBUG2,
         "PostFGA shmem: created hash '%s' (nelem=%d, keysize=%zu, entrysize=%zu)",
         name, nelem, keysize, entrysize);
}

/*
 * postfga_shmem_init_atomic_counters
 *
 * Initialize queue indices and stats counters.
 */
static void
postfga_shmem_init_atomic_counters(void)
{
    /* Queue indices */
    pg_atomic_init_u32(&shared_state->queue_head, 0);
    pg_atomic_init_u32(&shared_state->queue_tail, 0);
    pg_atomic_init_u32(&shared_state->queue_size, 0);

    /* Stats struct (별도 모듈에서 제공) */
    // stats_init(&shared_state->stats);

    elog(DEBUG2, "PostFGA shmem: atomic counters initialized");
}
