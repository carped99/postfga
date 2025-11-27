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
acquire_slot(FgaChannelSlotPool *pool)
{
    slist_node *node;
    FgaChannelSlot *slot;

    /* 빈 슬롯 없으면 에러 (또는 나중에 backoff/retry 설계 가능) */
    if (slist_is_empty(&pool->head))
        return NULL;

    node = slist_pop_head_node(&pool->head);
    slot = slist_container(FgaChannelSlot, node, node);

    pg_atomic_write_u32(&slot->state, FGA_CHECK_SLOT_PENDING);

    return slot;
}

static void
release_slot(FgaChannelSlotPool *pool, FgaChannelSlot *slot)
{
    pg_atomic_write_u32(&slot->state, FGA_CHECK_SLOT_EMPTY);
    slist_push_head(&pool->head, &slot->node);
}

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
        return NULL;
    }

    slot->backend_pid = MyProcPid;
    slot->request_id = pg_atomic_add_fetch_u64(&channel->request_id, 1);
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

FgaResponse *postfga_channel_execute(const FgaRequest *request)
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

    return &slot->response;
}

bool postfga_channel_check(const char *object_type,
                           const char *object_id,
                           const char *subject_type,
                           const char *subject_id,
                           const char *relation)
{
    FgaRequest request;
    MemSet(&request, 0, sizeof(request));
    request.type = FGA_REQUEST_CHECK_TUPLE;
    strncpy(request.body.checkTuple.store_id, "default", sizeof(request.body.checkTuple.store_id) - 1);
    strncpy(request.body.checkTuple.tuple.object_type, object_type, sizeof(request.body.checkTuple.tuple.object_type) - 1);
    strncpy(request.body.checkTuple.tuple.object_id, object_id, sizeof(request.body.checkTuple.tuple.object_id) - 1);
    strncpy(request.body.checkTuple.tuple.relation, relation, sizeof(request.body.checkTuple.tuple.relation) - 1);
    strncpy(request.body.checkTuple.tuple.subject_type, subject_type, sizeof(request.body.checkTuple.tuple.subject_type) - 1);
    strncpy(request.body.checkTuple.tuple.subject_id, subject_id, sizeof(request.body.checkTuple.tuple.subject_id) - 1);

    FgaResponse *response = postfga_channel_execute(&request);
    return response->body.checkTuple.allow;
}
