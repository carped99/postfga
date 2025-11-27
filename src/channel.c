/* channel_slot.c */

#include <postgres.h>
#include <miscadmin.h>
#include <storage/latch.h>
#include <storage/shmem.h>
#include <utils/elog.h>
#include <pgstat.h>
#include <lib/ilist.h>

#include "shmem.h"
#include "channel.h"

#define MAX_DRAIN 64

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------
 */
static FgaChannelSlot *
enqueue_slot(FgaChannel *channel)
{
    FgaChannelSlot *slot;
    FgaChannelSlotIndex index;

    LWLockAcquire(channel->lock, LW_EXCLUSIVE);
    slot = acquire_slot(channel->pool);

    if (slot == NULL)
    {
        LWLockRelease(channel->lock);
        ereport(LOG,
                errmsg("PostFGA: failed to acquire slot"));
        return NULL;
    }
    index = (slot - channel->pool->slots);
    Assert(index < channel->pool->size);

    if (!queue_enqueue_slot(channel->queue, index))
    {
        /* 큐가 가득 찼으면 슬롯 반환 후 실패 */
        release_slot(channel->pool, slot);
        LWLockRelease(channel->lock);
        return NULL;
    }
    LWLockRelease(channel->lock);
    return slot;
}

static FgaChannelSlot *
write_request(FgaChannel *channel,
              const FgaRequest *request)
{
    FgaChannelSlot *slot = enqueue_slot(channel);
    if (slot == NULL)
    {
        ereport(ERROR,
                errmsg("PostFGA: failed to enqueue request"));
    }

    slot->backend_pid = MyProcPid;
    slot->allowed = false;
    slot->error_code = 0;
    slot->request = *request;

    return slot;
}

static void
read_request(FgaChannel *channel,
             FgaChannelSlot *slot)
{
    FgaChannelSlotState state;

    Assert(channel != NULL);
    Assert(slot != NULL);
    Assert(slot->request_id != 0);
    Assert(slot->backend_pid == MyProcPid);

    PG_TRY();
    {
        do
        {
            int rc = WaitLatch(MyLatch,
                               WL_LATCH_SET | WL_EXIT_ON_PM_DEATH,
                               0,
                               PG_WAIT_EXTENSION);

            if (rc & WL_LATCH_SET)
                ResetLatch(MyLatch);

            CHECK_FOR_INTERRUPTS();

            state = (FgaChannelSlotState)
                pg_atomic_read_u32(&slot->state);

        } while (state != FGA_CHECK_SLOT_DONE && state != FGA_CHECK_SLOT_ERROR);
    }
    PG_CATCH();
    {
        release_slot(channel->pool, slot);
        PG_RE_THROW();
    }
    PG_END_TRY();

    release_slot(channel->pool, slot);
}

/*-------------------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------------------
 */
uint16
postfga_channel_drain_slots(FgaChannel *channel,
                            uint16 max_count,
                            FgaChannelSlot **out_slots)
{
    uint16 count;
    uint16 buf[MAX_DRAIN];

    /* 요청이 너무 크면 상한으로 잘라버림 */
    if (max_count > MAX_DRAIN)
        max_count = MAX_DRAIN;

    LWLockAcquire(channel->lock, LW_EXCLUSIVE);
    count = queue_dequeue_slots(channel->queue, buf, max_count);
    LWLockRelease(channel->lock);

    for (uint16 i = 0; i < count; ++i)
    {
        out_slots[i] = &channel->pool->slots[buf[i]];
    }

    return count;
}

bool postfga_channel_execute(const FgaRequest *request)
{
    PostfgaShmemState *state = postfga_get_shmem_state();
    FgaChannel *channel = state->channel;
    FgaChannelSlot *slot = write_request(channel, request);
    if (slot == NULL)
    {
        ereport(ERROR,
                errmsg("postfga: failed to enqueue check request"));
    }
    SetLatch(state->bgw_latch);
    read_request(channel, slot);

    return slot->allowed;
}
