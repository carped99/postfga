/* channel_slot.c */

#include <postgres.h>
#include <miscadmin.h>
#include <storage/latch.h>
#include <storage/shmem.h>
#include <utils/elog.h>
#include <pgstat.h>
#include <lib/ilist.h>

#include "shmem.h"
#include "check_channel.h"

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------
 */
static FgaCheckSlot *
acquire_slot(FgaCheckSlotPool *pool)
{
    slist_node *node;

    /* 빈 슬롯 없으면 에러 (또는 나중에 backoff/retry 설계 가능) */
    if (slist_is_empty(&pool->head))
        return NULL;

    node = slist_pop_head_node(&pool->head);
    return slist_container(FgaCheckSlot, node, node);
}

static void
release_slot(FgaCheckSlotPool *pool, FgaCheckSlot *slot)
{
    pg_atomic_write_u32(&slot->state, FGA_CHECK_SLOT_EMPTY);
    slist_push_head(&pool->head, &slot->node);
}

/*-------------------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------------------
 */
FgaCheckSlot *
postfga_check_write(FgaCheckChannel *channel,
                    const FgaCheckTupleRequest *request)
{
    FgaCheckSlot *slot;
    FgaCheckSlotIndex slot_index;
    LWLockAcquire(channel->lock, LW_EXCLUSIVE);
    slot = acquire_slot(channel->pool);

    if (slot == NULL)
    {
        LWLockRelease(channel->lock);
        return NULL;
    }

    slot_index = (slot - channel->pool->slots);
    Assert(slot_index < channel->pool->size);

    if (!queue_enqueue_slot(channel->queue, slot_index))
    {
        /* 큐가 가득 찼으면 슬롯 반환 후 실패 */
        release_slot(channel->pool, slot);
        LWLockRelease(channel->lock);
        return NULL;
    }
    LWLockRelease(channel->lock);

    slot->backend_pid = MyProcPid;
    slot->allowed = false;
    slot->error_code = 0;
    slot->request = *request;

    pg_atomic_write_u32(&slot->state,
                        FGA_CHECK_SLOT_PENDING);
    return slot;
}

void postfga_check_read(FgaCheckChannel *channel,
                        FgaCheckSlot *slot)
{
    Latch *latch;
    FgaCheckSlotState state;

    Assert(channel != NULL);
    Assert(slot != NULL);
    Assert(slot->backend_pid == MyProcPid);

    for (;;)
    {
        int rc = WaitLatch(MyLatch,
                           WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
                           1000L, /* 1초마다 폴링 */
                           PG_WAIT_EXTENSION);

        if (rc & WL_LATCH_SET)
            ResetLatch(MyLatch);

        CHECK_FOR_INTERRUPTS();

        state = (FgaCheckSlotState)
            pg_atomic_read_u32(&slot->state);

        if (state == FGA_CHECK_SLOT_DONE ||
            state == FGA_CHECK_SLOT_ERROR)
            break;
    }

    /* 여기까지 오면 BGW가 allowed/error_code 채워둔 상태 */

    FgaCheckTupleRequest *req = &slot->request;

    /* 결과를 요청이 가리키는 출력 포인터에 복사 */
    // if (req->out_allowed)
    //     *(req->out_allowed) = slot->allowed;

    // if (req->out_error_code)
    //     *(req->out_error_code) = slot->error_code;

    /* 슬롯 풀로 반환 */
    LWLockAcquire(channel->lock, LW_EXCLUSIVE);

    pg_atomic_write_u32(&slot->state,
                        FGA_CHECK_SLOT_EMPTY);
    slist_push_head(&channel->pool->head, &slot->node);

    LWLockRelease(channel->lock);

    /* 에러 처리 정책:
     * - 여기서 ereport(ERROR) 를 던질지
     * - 호출자가 req->out_error_code 보고 판단하게 둘지는
     *   네가 전역 정책으로 한 번만 정하면 됨.
     */
    if (slot->error_code != 0)
    {
        ereport(ERROR,
                (errmsg("PostFGA check failed, error code %d",
                        slot->error_code)));
    }
}

static Size
channel_shmem_size()
{
    return MAXALIGN(sizeof(FgaCheckChannel));
}

static Size
pool_shmem_size(uint32 capacity)
{
    return MAXALIGN(offsetof(FgaCheckSlotPool, slots) + sizeof(FgaCheckSlot) * capacity);
}

static Size
queue_shmem_size(uint32 capacity)
{
    return MAXALIGN(offsetof(FgaCheckSlotQueue, values) + sizeof(uint16) * capacity);
}

static void
pool_init(FgaCheckSlotPool *pool, uint32 max_slots)
{
    uint32 i;

    slist_init(&pool->head);

    for (i = 0; i < max_slots; i++)
    {
        FgaCheckSlot *slot = &pool->slots[i];

        slist_push_head(&pool->head, &slot->node);

        pg_atomic_init_u32(&slot->state, FGA_CHECK_SLOT_EMPTY);
        slot->backend_pid = InvalidPid;
        slot->request_id = 0;
        MemSet(&slot->request, 0, sizeof(FgaCheckTupleRequest));
        slot->allowed = false;
        slot->error_code = 0;
    }
}

Size postfga_check_channel_shmem_size(uint32 slot_count)
{
    elog(LOG, "PostFGA: init check channel with slot_count=%u", slot_count);

    Size size = channel_shmem_size();

    // pool
    size = add_size(size, pool_shmem_size(slot_count));

    // queue
    size = add_size(size, queue_shmem_size(slot_count));
    return size;
}

void postfga_check_channel_shmem_init(uint32 slot_count)
{
    bool found;
    LWLockPadded *locks;
    Size size = postfga_check_channel_shmem_size(slot_count);

    FgaCheckChannel *ch = (FgaCheckChannel *)
        ShmemInitStruct("PostFGA Check Channel", size, &found);

    if (found)
        return;

    {
        FgaCheckSlotPool *pool;
        FgaCheckSlotQueue *queue;

        // Channel struct
        char *ptr = (char *)ch + channel_shmem_size();

        // pool
        pool = (FgaCheckSlotPool *)ptr;
        ptr += pool_shmem_size(slot_count);

        // queue
        queue = (FgaCheckSlotQueue *)ptr;
        ptr += queue_shmem_size(slot_count);

        Assert(ptr == (char *)ch + size);

        ch->pool = pool;
        ch->queue = queue;

        pool_init(ch->pool, slot_count);
        queue_init(ch->queue, slot_count);
    }
}