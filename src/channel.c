/* channel_slot.c */

#include <postgres.h>

#include <miscadmin.h>
#include <pgstat.h>
#include <storage/latch.h>
#include <storage/proc.h>
#include <storage/procarray.h>
#include <storage/shmem.h>
#include <utils/elog.h>

#include "channel.h"
#include "channel_slot.h"
#include "state.h"

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------
 */
// static FgaChannelSlot* write_request(FgaChannel* const channel, const FgaRequest* request)
// {
//     FgaChannelSlot* slot;
//     FgaChannelSlotIndex index;

//     /* 1) slot만 freelist에서 가져오기 */
//     LWLockAcquire(channel->pool_lock, LW_EXCLUSIVE);
//     slot = acquire_slot(channel->pool);
//     LWLockRelease(channel->pool_lock);
//     if (slot == NULL)
//     {
//         return NULL;
//     }

//     index = (slot - channel->pool->slots);
//     Assert(index < channel->pool->size);

//     /* 2) slot 내용 먼저 다 쓰기 */
//     slot->backend_pid = MyProcPid;
//     slot->payload.request = *request;
//     slot->payload.request.request_id = pg_atomic_add_fetch_u64(&channel->request_id, 1);

//     LWLockAcquire(channel->queue_lock, LW_EXCLUSIVE);
//     if (!queue_enqueue(channel->queue, index))
//     {
//         /* 롤백 처리 */
//         LWLockRelease(channel->queue_lock);
//         fga_channel_release_slot(channel, slot);
//         return NULL;
//     }
//     LWLockRelease(channel->queue_lock);
//     return slot;
// }

static FgaChannelSlotState wait_response(FgaChannel* channel, FgaChannelSlot* slot)
{
    int rc;
    FgaChannelSlotState state;

    Assert(channel != NULL);
    Assert(slot != NULL);
    Assert(slot->backend_pid == MyProcPid);

    PG_TRY();
    {
        for (;;)
        {
            state = (FgaChannelSlotState)pg_atomic_read_u32(&slot->state);
            if (state == FGA_CHANNEL_SLOT_DONE || state == FGA_CHANNEL_SLOT_CANCELED)
                break;

            rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, -1, PG_WAIT_EXTENSION);

            if (rc & WL_LATCH_SET)
                ResetLatch(MyLatch);

            CHECK_FOR_INTERRUPTS();
        }
    }
    PG_CATCH();
    {
        FgaChannelSlotState cur = (FgaChannelSlotState)pg_atomic_read_u32(&slot->state);

        if (cur == FGA_CHANNEL_SLOT_PENDING || cur == FGA_CHANNEL_SLOT_PROCESSING)
        {
            /* 아직 BGW가 처리 중일 가능성이 있으니, CANCEL 표시만 한다 */
            pg_atomic_write_u32(&slot->state, FGA_CHANNEL_SLOT_CANCELED);
        }
        else if (cur == FGA_CHANNEL_SLOT_DONE)
        {
            /* 이미 처리 완료된 상태였으면 여기서 정리해도 됨 */
            fga_channel_release_slot(slot);
        }
        slot->backend_pid = InvalidPid;
        /* freelist로는 아직 돌리지 않는다. BGW에서 처리 */
        PG_RE_THROW();
    }
    PG_END_TRY();

    return state;
}

/*-------------------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------------------
 */
FgaChannelSlot* fga_channel_acquire_slot(void)
{
    FgaChannel* const channel = fga_get_channel();
    FgaChannelSlot* slot;
    

    LWLockAcquire(channel->pool_lock, LW_EXCLUSIVE);
    slot = acquire_slot(channel->pool);
    LWLockRelease(channel->pool_lock);

    if (slot == NULL)
    {
       ereport(ERROR,
                (errcode(ERRCODE_OUT_OF_MEMORY),
                 errmsg("channel slot pool exhausted"),
                 errhint("Increase postfga.max_slots or check for slot leaks")));
    }

    slot->backend_pid = MyProcPid;

    // Reset payload
    MemSet(&slot->payload, 0, sizeof(slot->payload));

    slot->payload.request.request_id = pg_atomic_add_fetch_u64(&channel->request_id, 1);

    return slot;
}

void fga_channel_release_slot(FgaChannelSlot* slot)
{
    FgaChannel* const channel = fga_get_channel();
    LWLockAcquire(channel->pool_lock, LW_EXCLUSIVE);
    release_slot(channel->pool, slot);
    LWLockRelease(channel->pool_lock);
}

uint32 fga_channel_drain_slots(uint32 max_count, FgaChannelSlot** out_slots)
{
    uint32 count;
    uint32 buf[FGA_CHANNEL_DRAIN_MAX];
    FgaChannel* const channel = fga_get_channel();

    /* 요청이 너무 크면 상한으로 잘라버림 */
    if (max_count > FGA_CHANNEL_DRAIN_MAX)
        max_count = FGA_CHANNEL_DRAIN_MAX;
        
    LWLockAcquire(channel->queue_lock, LW_EXCLUSIVE);
    count = queue_drain(channel->queue, max_count, buf);
    LWLockRelease(channel->queue_lock);

    for (uint32 i = 0; i < count; ++i)
    {
        out_slots[i] = &channel->pool->slots[buf[i]];
    }

    return count;
}

void fga_channel_execute_slot(FgaChannelSlot* slot)
{
    FgaChannel* const channel = fga_get_channel();
    FgaChannelSlotIndex index = (slot - channel->pool->slots);
    FgaChannelSlotState slot_state;
    
    Assert(index < channel->pool->size);

    LWLockAcquire(channel->queue_lock, LW_EXCLUSIVE);
    if (!queue_enqueue(channel->queue, index))
    {
        /* 롤백 처리 */
        LWLockRelease(channel->queue_lock);
        fga_channel_release_slot(slot);
        ereport(ERROR, errmsg("postfga: failed to enqueue channel slot"));
    }
    LWLockRelease(channel->queue_lock);

    /* BGW 깨우기 */
    fga_wake_bgw();

    slot_state = wait_response(channel, slot);

    if (slot_state == FGA_CHANNEL_SLOT_CANCELED)
    {
        /* 여기까지 왔으면 논리적으로는 이미 위에서 ERROR가 났을 확률이 높지만,
         * 방어 코드로 이렇게 한 번 더 정리해줄 수 있음.
         */
        fga_channel_release_slot(slot);
        ereport(ERROR, errmsg("postfga: request was canceled"));
    }

    pg_read_barrier();
}

// void fga_channel_execute(const FgaRequest* request, FgaResponse* response)
// {
//     FgaChannel* const channel = fga_get_channel();
//     FgaChannelSlot* slot = write_request(channel, request);
//     FgaChannelSlotState slot_state;

//     if (slot == NULL)
//     {
//         ereport(ERROR, errmsg("postfga: failed to enqueue check request"));
//     }

//     /* BGW 깨우기 */
//     fga_wake_bgw();

//     slot_state = wait_response(channel, slot);

//     if (slot_state == FGA_CHANNEL_SLOT_CANCELED)
//     {
//         /* 여기까지 왔으면 논리적으로는 이미 위에서 ERROR가 났을 확률이 높지만,
//          * 방어 코드로 이렇게 한 번 더 정리해줄 수 있음.
//          */
//         fga_channel_release_slot(channel, slot);
//         ereport(ERROR, errmsg("postfga: request was canceled"));
//     }

//     pg_read_barrier();

//     // copy response
//     *response = slot->payload.response;

//     fga_channel_release_slot(channel, slot);
// }

bool fga_channel_wake_backend(const FgaChannelSlot* slot)
{
    PGPROC* proc = BackendPidGetProc(slot->backend_pid);
    if (proc != NULL && proc->pid == slot->backend_pid)
    {
        SetLatch(&proc->procLatch);
        return true;
    }
    return false;
}
