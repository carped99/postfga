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
#include "config.h"
#include "shmem.h"

/* Named LWLock tranche 이름과 필요한 락 개수 */
#define POSTFGA_LWLOCK_TRANCHE_NAME "postfga"
#define POSTFGA_LWLOCK_TRANCHE_NUM 3

static int max_cache_entries = DEFAULT_CACHE_ENTRIES;

/* Global shared memory state pointer */
static PostfgaShmemState *postfga_shmem_state = NULL;

/* GUC (guc.c 에서 정의한다고 가정) */
int postfga_l2_cache_size = 16384;

/*-------------------------------------------------------------------------
 * Static helpers (private)
 *-------------------------------------------------------------------------*/
static Size postfga_shmem_calculate_size(void);
static void postfga_shmem_initialize_state(bool found);

static Size postfga_shmem_channel_size(void);
// static void postfga_shmem_request_slots_init(void);
// static Size postfga_shmem_request_queue_size(void);

static Size postfga_shmem_cache_size(void);
static void postfga_shmem_cache_init(PostfgaShmemState *state);
static uint64_t postfga_generate_hash_seed();

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

    ereport(LOG, (errmsg("PostFGA: starting shared memory")));

    // if (postfga_shmem_state != NULL)
    //     return;

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    size = postfga_shmem_calculate_size();

    postfga_shmem_state = ShmemInitStruct("PostFGA Data", size, &found);

    // ereport(LOG, (errmsg("PostFGA: 3. setting hash seed")));
    // postfga_shmem_state->hash_seed = postfga_generate_hash_seed();

    /* 실제 필드 초기화 */
    postfga_shmem_initialize_state(found);
    LWLockRelease(AddinShmemInitLock);

    ereport(LOG,
            (errmsg("PostFGA: shared memory startup complete (found=%s)",
                    found ? "true" : "false")));
}

/*-------------------------------------------------------------------------
 * Public  API
 *-------------------------------------------------------------------------*/
PostfgaShmemState *postfga_get_shmem_state(void)
{
    return postfga_shmem_state;
}

FgaL2Cache *postfga_get_cache_state(void)
{
    return &postfga_shmem_state->l2_cache;
}

static Size
postfga_shmem_calculate_size(void)
{
    PostfgaConfig *cfg = postfga_get_config();
    Size size = 0;

    /* 1. shmem state struct */
    size = MAXALIGN(sizeof(PostfgaShmemState));

    /* 2. request_slot */
    size = add_size(size, postfga_check_channel_shmem_size(cfg->max_slots));

    // size = add_size(size, postfga_check_channel_shmem_size());

    /* 3. request_queue */
    // size = add_size(size, postfga_shmem_request_queue_size());

    /* 4. L2 cache */
    size = add_size(size, postfga_shmem_cache_size());

    return size;
}

/*
 * postfga_shmem_initialize_state
 *
 *   - found == false 이면 shared memory 처음 attach 된 것 → 전체 초기화
 *   - found == true 이면 이미 다른 프로세스가 초기화한 상태 → 포인터/락만 다시 세팅
 */
static void
postfga_shmem_initialize_state(bool found)
{
    PostfgaConfig *cfg = postfga_get_config();
    LWLockPadded *locks;
    char *ptr;
    int i;

    if (found)
    {
        /* 이미 다른 backend가 초기화 완료 */
        return;
    }

    /* Named LWLock tranche에서 락 배열 가져오기 */
    ereport(LOG, (errmsg("PostFGA: 4. GetNamedLWLockTranche")));
    locks = GetNamedLWLockTranche(POSTFGA_LWLOCK_TRANCHE_NAME);

    /* state 내 락 포인터 설정 */
    postfga_shmem_state->lock = &locks[0].lock;

    /* 전체 영역은 ShmemInitStruct 가 이미 zero 로 초기화해 줌 */
    postfga_shmem_state->bgw_latch = NULL;
    postfga_shmem_state->hash_seed = postfga_generate_hash_seed();

    ptr = (char *)postfga_shmem_state;

    /* 1. shmem state struct */
    ptr += MAXALIGN(sizeof(PostfgaShmemState));

    postfga_shmem_state->check_channel = (FgaCheckChannel *)ptr;
    ptr += postfga_check_channel_shmem_size(cfg->max_slots);

    /* 3. request_queue */
    // postfga_shmem_state->request_queue = (FgaRequestQueue *)ptr;
    // ptr += postfga_shmem_request_queue_size();

    postfga_check_channel_shmem_init(cfg->max_slots);
    /* 4. L2 cache */
    postfga_shmem_state->l2_cache.lock = &locks[2].lock;
    postfga_shmem_cache_init(postfga_shmem_state);
}

// void postfga_shmem_request_slots_init()
// {
//     PostfgaConfig *cfg = postfga_get_config();
//     for (uint16 i = 0; i < cfg->max_slots; i++)
//     {
//         FgaRequestSlot *slot = &postfga_shmem_state->request_slots[i];
//         pg_atomic_init_u32(&slot->state, FGA_SLOT_EMPTY);

//         MemSet(&slot->request, 0, sizeof(FgaRequest));
//         MemSet(&slot->response, 0, sizeof(FgaResponse));
//     }
// }

// Size postfga_shmem_request_queue_size()
// {
//     PostfgaConfig *cfg = postfga_get_config();
//     return MAXALIGN(offsetof(FgaRequestQueue, slots) + sizeof(uint16_t) * cfg->max_slots);
// }

Size postfga_shmem_cache_size()
{
    PostfgaConfig *cfg = postfga_get_config();
    Size size = 0;

    size = add_size(size, hash_estimate_size(cfg->max_cache_entries, sizeof(FgaAclCacheEntry)));

    size = add_size(size, hash_estimate_size(cfg->max_relations, sizeof(FgaRelationCacheEntry)));

    return size;
}

void postfga_shmem_cache_init(PostfgaShmemState *state)
{
    PostfgaConfig *cfg = postfga_get_config();
    FgaL2Cache *cache = &state->l2_cache;

    HASHCTL ctl;
    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(FgaAclCacheKey);
    ctl.entrysize = sizeof(FgaAclCacheEntry);
    cache->acl = ShmemInitHash("PostFGA L2 cache",
                               cfg->max_cache_entries, /* init_size */
                               cfg->max_cache_entries, /* max_size  */
                               &ctl,
                               HASH_ELEM | HASH_BLOBS | HASH_SHARED_MEM);

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.entrysize = sizeof(FgaRelationCacheEntry);
    cache->relation = ShmemInitHash("PostFGA Relation cache",
                                    cfg->max_relations, /* init_size */
                                    cfg->max_relations, /* max_size  */
                                    &ctl,
                                    HASH_ELEM | HASH_STRINGS | HASH_SHARED_MEM);
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

/*-------------------------------------------------------------------------
 * Private helpers
 *-------------------------------------------------------------------------*/
uint64_t
postfga_generate_hash_seed()
{
    uint64_t seed;
    if (!pg_strong_random(&seed, sizeof(seed)))
    {
        ereport(ERROR,
                (errmsg("PostFGA: failed to generate hash seed")));
    }
    return seed;
}
