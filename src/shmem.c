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

#include <postgres.h>
#include <storage/ipc.h>
#include <storage/lwlock.h>
#include <storage/shmem.h>
#include <utils/hsearch.h>

#include "postfga.h"
#include "shmem.h"
#include "queue.h"
#include "cache.h"
#include "stats.h"

/* Named LWLock tranche 이름과 필요한 락 개수 */
#define POSTFGA_LWLOCK_TRANCHE_NAME "postfga"
#define POSTFGA_LWLOCK_TRANCHE_NUM 2 /* 0: global, 1: cache */

/*-------------------------------------------------------------------------
 * Module-level state
 *-------------------------------------------------------------------------*/
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
static int max_cache_entries = DEFAULT_CACHE_ENTRIES;

/* GUC (guc.c 에서 정의한다고 가정) */
int postfga_max_slots = 1024;
int postfga_queue_capacity = 1024;
int postfga_l2_cache_size = 16384;

/*-------------------------------------------------------------------------
 * Static helpers (private)
 *-------------------------------------------------------------------------*/
static Size postfga_shmem_calculate_size(void);
static void postfga_shmem_initialize_state(PostfgaShmemState *state, bool found);

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
                       (postfga_qsize_t)MAX_PENDING_REQ);

    /* Stats 초기화 */
    postfga_init_stats(&shared_state->stats);

    elog(DEBUG1, "PostFGA shmem: shared_state initialization complete");
}

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
void postfga_shmem_request(void)
{
    Size size = postfga_shmem_calculate_size();

    RequestAddinShmemSpace(size);
    RequestNamedLWLockTranche(POSTFGA_LWLOCK_TRANCHE_NAME,
                              POSTFGA_LWLOCK_TRANCHE_NUM);

    elog(DEBUG1,
         "PostFGA shmem: requested %zu bytes (max_cache_entries=%d)",
         size,
         max_cache_entries);
}

/*
 * postfga_shmem_startup
 *
 * Called from shmem_startup_hook after PostgreSQL allocates shared memory.
 *
 * - Uses AddinShmemInitLock to ensure single initialization
 * - Creates or attaches to shared memory segment
 * - Initializes cache, queue, counters, and stats
 */
void postfga_shmem_startup(void)
{
    bool found = false;
    Size size;
    LWLockPadded *locks;

    /* 이미 초기화 돼 있으면 재진입 방지 */
    if (PostfgaSharedState != NULL)
        return;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    size = postfga_shmem_calculate_size();

    PostfgaSharedState = ShmemInitStruct("PostFGA Shmem State", size, &found);

    /* Named LWLock tranche에서 락 배열 가져오기 */
    locks = GetNamedLWLockTranche(POSTFGA_LWLOCK_TRANCHE_NAME);

    /* state 내 락 포인터 설정 */
    PostfgaSharedState->lock = &locks[0].lock;
    PostfgaSharedState->l2_cache.lock = &locks[1].lock;

    /* 실제 필드 초기화 */
    postfga_shmem_initialize_state(PostfgaSharedState, found);

    LWLockRelease(AddinShmemInitLock);
}

static Size
postfga_shmem_calculate_size(void)
{
    Size size = 0;
    int max_slots = Max(postfga_max_slots, 1024);
    int queue_capacity = Max(postfga_queue_capacity, max_slots);

    /* 기본 state struct */
    size = MAXALIGN(sizeof(PostfgaShmemState));

    /* 슬롯 배열 */
    size = add_size(size, MAXALIGN(mul_size(max_slots, sizeof(FgaSlot))));

    /* RequestQueue (flexible array 포함) */
    {
        Size qsize = offsetof(RequestQueue, entries) + sizeof(uint16) * (Size)queue_capacity;
        size = add_size(size, MAXALIGN(qsize));
    }

    /* L2 캐시 hash table */
    size = add_size(size, MAXALIGN(postfga_l2_estimate_shmem_size(postfga_l2_cache_size)));

    return size;
}

static void
postfga_shmem_initialize_state(PostfgaShmemState *state, bool found)
{
    char *ptr;
    int i;
    int max_slots = Max(postfga_max_slots, 1024);
    int queue_capacity = Max(postfga_queue_capacity, max_slots);

    if (found)
    {
        /* 이미 다른 backend가 초기화 완료 */
        return;
    }

    /* 전체 영역은 ShmemInitStruct 가 이미 zero 로 초기화해 줌 */

    state->bgw_latch = NULL;
    // state->max_slots = (uint32)max_slots;
    // state->queue_capacity = (uint32)queue_capacity;
    // state->total_requests = 0;
    // state->total_cache_hits = 0;

    /* PostfgaShmemState 바로 뒤에 slots[] / RequestQueue / L2 hash 순으로 배치 */
    ptr = (char *)state;
    ptr += MAXALIGN(sizeof(PostfgaShmemState));

    /* 슬롯 배열 배치 */
    state->slots = (FgaSlot *)ptr;
    ptr += MAXALIGN(mul_size(max_slots, sizeof(FgaSlot)));

    /* RequestQueue 배치 */
    {
        Size qsize = offsetof(RequestQueue, entries) + sizeof(uint16) * (Size)queue_capacity;

        state->request_queue = (RequestQueue *)ptr;
        ptr += MAXALIGN(qsize);

        /* 큐 초기화 */
        memset(state->request_queue, 0, qsize);
        state->request_queue->capacity = (uint32)queue_capacity;
        state->request_queue->head = 0;
        state->request_queue->tail = 0;
        state->request_queue->size = 0;

        /* capacity 가 2의 제곱이면 mask 세팅, 아니면 0 */
        if ((queue_capacity & (queue_capacity - 1)) == 0)
            state->request_queue->mask = (uint32)(queue_capacity - 1);
        else
            state->request_queue->mask = 0;
    }

    /* 슬롯 상태 초기화 */
    for (i = 0; i < max_slots; i++)
    {
        FgaSlot *slot = &state->slots[i];

        pg_atomic_init_u32(&slot->state, FGA_SLOT_EMPTY);
        slot->generation = 0;
    }

    /* L2 캐시 초기화 (ptr 이후에는 L2 hash table 영역이 이어짐) */
    postfga_l2_init(&state->l2_cache,
                    postfga_l2_cache_size,
                    state->l2_cache.lock);
}

Cache *
postfga_get_cache_state(void)
{
    return &PostfgaSharedState->cache;
}

/*
 * Optional: GUC hook 이나 다른 초기화 코드에서 호출해서
 * max_cache_entries 값을 갱신하고 싶다면 이 헬퍼를 사용할 수 있다.
 */
void postfga_set_max_cache_entries(int value)
{
    if (value <= 0)
        elog(WARNING,
             "PostFGA shmem: invalid max_cache_entries=%d, keeping %d",
             value,
             max_cache_entries);
    else
        max_cache_entries = value;
}
