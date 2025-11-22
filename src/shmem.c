/*-------------------------------------------------------------------------
 *
 * postfga_shmem.c
 *    Shared memory lifecycle management for PostFGA extension.
 *
 * Responsibilities:
 *   - Shared memory size estimation
 *   - Global shared state allocation and initialization
 *   - Response cache initialization
 *   - Request queue initialization
 *   - Statistics initialization
 *
 * This file owns the single global PostfgaShmemState instance.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/hsearch.h"

#include "common.h"          /* DEFAULT_CACHE_ENTRIES, MAX_PENDING_REQ 등 */
#include "shmem.h"           /* PostfgaShmemState, postfga_get_shared_state 선언 헤더 */
#include "queue.h"           /* PostfgaRequestQueue, postfga_init_queue */
#include "cache.h"           /* ResponseCache, postfga_init_cache */
#include "stats.h"           /* Stats, stats_init */

/*-------------------------------------------------------------------------
 * Module-level state
 *-------------------------------------------------------------------------*/

/*
 * Global pointer to shared state.
 * - Set exactly once in postfga_startup_shmem()
 * - Read via postfga_get_shared_state()
 */
static PostfgaShmemState *shared_state = NULL;

/*
 * GUC-backed configuration.
 *
 * max_cache_entries 값은 다른 모듈에서 GUC로 설정될 수 있고,
 * 여기서는 "현재 설정된 값"을 사용해 shared memory 크기를 계산한다.
 *
 * 실제 GUC 등록은 별도 guc.c(or config.c) 에서:
 *   DefineCustomIntVariable("postfga.max_cache_entries", ...)
 * 등을 통해 처리하고, 해당 GUC의 hook에서 이 static 변수를 갱신해도 된다.
 */
static int  max_cache_entries = DEFAULT_CACHE_ENTRIES;

/*-------------------------------------------------------------------------
 * Static helpers (private)
 *-------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------
 * Static helper implementations
 *-------------------------------------------------------------------------*/

/*
 * _calculate_size
 *
 * Estimate the total shared memory size required for PostFGA.
 *
 * 구성:
 *   - PostfgaShmemState 구조체 자체
 *   - ResponseCache 내부에서 사용하는 해시 테이블들
 *   - Request queue용 RequestPayload 배열
 */
static Size
_calculate_size(void)
{
    Size size = 0;

    /* Main shared state structure */
    size = add_size(size, sizeof(PostfgaShmemState));

    /*
     * Response cache의 해시 테이블 등.
     *
     * cache 모듈은 자신의 내부 구조에 필요한 shared memory 크기를
     * postfga_cache_estimate_size() 로 계산한다.
     */
    size = add_size(size, postfga_cache_estimate_size(max_cache_entries));

    /*
     * Request queue (링버퍼용 RequestPayload 배열)
     *
     * - MAX_PENDING_REQ 는 "슬롯 수" (요청 최대 개수)
     * - PostfgaRequestQueue 자체는 PostfgaShmemState에 포함되어 있으므로
     *   여기서는 순수히 RequestPayload 배열 크기만 계산하면 된다.
     */
    size = add_size(size,
                    mul_size(MAX_PENDING_REQ,
                             sizeof(RequestPayload)));

    return size;
}


/*
 * postfga_initialize_state
 *
 * Zero out the shared_state structure, set up locks,
 * initialize cache, queue, and stats.
 */
static void
_initialize(void)
{
    RequestPayload *queue_storage;

    memset(shared_state, 0, sizeof(PostfgaShmemState));

    /* Initialize LWLock from named tranche ("postfga", 1) */
    shared_state->lock = &GetNamedLWLockTranche("postfga")->lock;

    /* BGW latch 는 BGW 시작 시점에 설정됨 (bgw_main 에서 등록) */
    shared_state->bgw_latch = NULL;

    /* Generation counter 시작값 */
    shared_state->next_generation = 1;

    /* Response cache 초기화 */
    postfga_init_cache(&shared_state->cache, max_cache_entries);

    /* Request queue allocation & init */
    queue_storage = (RequestPayload *)
        ShmemAlloc(mul_size(MAX_PENDING_REQ, sizeof(RequestPayload)));

    if (queue_storage == NULL)
        elog(ERROR, "PostFGA shmem: failed to allocate request queue storage");

    memset(queue_storage, 0, mul_size(MAX_PENDING_REQ, sizeof(RequestPayload)));

    postfga_init_queue(&shared_state->request_queue,
                       queue_storage,
                       (postfga_qsize_t) MAX_PENDING_REQ);

    /* Stats 초기화 */
    postfga_init_stats(&shared_state->stats);

    elog(DEBUG1, "PostFGA shmem: shared_state initialization complete");
}


/*-------------------------------------------------------------------------
 * Public lifecycle API
 *-------------------------------------------------------------------------*/

/*
 * postfga_request_shmem
 *
 * Called from _PG_init() before PostgreSQL allocates shared memory.
 *
 * - Estimates total shared memory size for this extension
 * - Requests allocation via RequestAddinShmemSpace()
 * - Registers a named LWLock tranche for synchronization
 */
void
postfga_request_shmem(void)
{
    Size shmem_size = _calculate_size();

    RequestAddinShmemSpace(shmem_size);
    RequestNamedLWLockTranche("postfga", 1);

    elog(DEBUG1,
         "PostFGA shmem: requested %zu bytes (max_cache_entries=%d)",
         shmem_size,
         max_cache_entries);
}

/*
 * postfga_startup_shmem
 *
 * Called from shmem_startup_hook after PostgreSQL allocates shared memory.
 *
 * - Uses AddinShmemInitLock to ensure single initialization
 * - Creates or attaches to shared memory segment
 * - Initializes cache, queue, counters, and stats
 */
void
postfga_startup_shmem(void)
{
    bool found = false;

    elog(DEBUG1, "PostFGA shmem: startup");

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    shared_state = ShmemInitStruct("PostFGA Shared State",
                                   sizeof(PostfgaShmemState),
                                   &found);

    if (shared_state == NULL)
    {
        LWLockRelease(AddinShmemInitLock);
        elog(ERROR, "PostFGA shmem: ShmemInitStruct returned NULL");
    }

    if (!found)
    {
        /* First-time initialization */
        _initialize();
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

Cache *
postfga_get_cache_state(void)
{
    return &shared_state->cache;
}

/*
 * Optional: GUC hook 이나 다른 초기화 코드에서 호출해서
 * max_cache_entries 값을 갱신하고 싶다면 이 헬퍼를 사용할 수 있다.
 */
void
postfga_set_max_cache_entries(int value)
{
    if (value <= 0)
        elog(WARNING,
             "PostFGA shmem: invalid max_cache_entries=%d, keeping %d",
             value,
             max_cache_entries);
    else
        max_cache_entries = value;
}
