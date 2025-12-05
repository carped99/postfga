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

#include "cache_shmem.h"
#include "channel_shmem.h"
#include "shmem.h"

/* Named LWLock tranche 이름과 필요한 락 개수 */
#define POSTFGA_LWLOCK_TRANCHE_NAME "postfga"
#define POSTFGA_LWLOCK_TRANCHE_NUM 4

/* Global shared memory state pointer */
PostfgaShmemState* postfga_shmem_state_instance_ = NULL;

/*-------------------------------------------------------------------------
 * Static helpers (private)
 *-------------------------------------------------------------------------*/
static uint64_t _generate_hash_seed()
{
    uint64_t seed;
    if (!pg_strong_random(&seed, sizeof(seed)))
    {
        ereport(ERROR, errmsg("failed to generate hash seed"));
    }
    return seed;
}

static Size _calculate_size(void)
{
    Size size = 0;

    /* 1. shared memory state struct */
    size = MAXALIGN(sizeof(PostfgaShmemState));

    /* 2. channel */
    size = add_size(size, MAXALIGN(postfga_channel_shmem_size()));

    // 3. L2 cache
    size = add_size(size, MAXALIGN(postfga_cache_shmem_size()));

    return size;
}

/*
 * _initialize_state
 *
 *   - found == false 이면 shared memory 처음 attach 된 것 → 전체 초기화
 *   - found == true 이면 이미 다른 프로세스가 초기화한 상태 → 포인터/락만 다시 세팅
 */
static void _initialize_state(void)
{
    /* Named LWLock tranche에서 락 배열 가져오기 */
    LWLockPadded* locks = GetNamedLWLockTranche(POSTFGA_LWLOCK_TRANCHE_NAME);
    char* ptr = (char*)postfga_shmem_state_instance_;

    /* state 내 락 포인터 설정 */
    postfga_shmem_state_instance_->lock = &locks[0].lock;

    /* 전체 영역은 ShmemInitStruct 가 이미 zero 로 초기화해 줌 */
    postfga_shmem_state_instance_->bgw_latch = NULL;
    postfga_shmem_state_instance_->hash_seed = _generate_hash_seed();

    /* 1. shmem state struct */
    ptr += MAXALIGN(sizeof(PostfgaShmemState));

    postfga_shmem_state_instance_->channel = (FgaChannel*)ptr;
    ptr += postfga_channel_shmem_size();

    postfga_channel_shmem_init(postfga_shmem_state_instance_->channel, &locks[1].lock, &locks[2].lock);

    /* 2. L2 cache */
    postfga_cache_shmem_init(&postfga_shmem_state_instance_->cache, &locks[3].lock);
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
    Size size = _calculate_size();

    ereport(LOG, errmsg("2. PostFGA: request shared memory (size=%zu bytes)", size));

    RequestAddinShmemSpace(size);
    RequestNamedLWLockTranche(POSTFGA_LWLOCK_TRANCHE_NAME, POSTFGA_LWLOCK_TRANCHE_NUM);
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
    Size size = _calculate_size();

    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    postfga_shmem_state_instance_ = ShmemInitStruct("PostFGA Data", size, &found);

    if (!found)
    {
        /* 실제 필드 초기화 */
        _initialize_state();
    }

    LWLockRelease(AddinShmemInitLock);
}
