/* channel_slot.c */

#include <postgres.h>

#include <lib/ilist.h>
#include <miscadmin.h>
#include <pgstat.h>
#include <storage/latch.h>
#include <storage/shmem.h>
#include <utils/elog.h>

#include "channel.h"
#include "shmem.h"

#define MAX_DRAIN 64

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------
 */
static FgaChannelSlot* acquire_slot(FgaChannelSlotPool* pool)
{
    slist_node* node;
    FgaChannelSlot* slot;

    /* 빈 슬롯 없으면 에러 (또는 나중에 backoff/retry 설계 가능) */
    if (slist_is_empty(&pool->head))
        return NULL;

    node = slist_pop_head_node(&pool->head);
    slot = slist_container(FgaChannelSlot, node, node);
    pg_atomic_write_u32(&slot->state, FGA_CHANNEL_SLOT_PENDING);
    return slot;
}

static void release_slot(FgaChannelSlotPool* pool, FgaChannelSlot* slot)
{
    pg_atomic_write_u32(&slot->state, FGA_CHANNEL_SLOT_EMPTY);
    slist_push_head(&pool->head, &slot->node);
}

static FgaChannelSlot* write_request(FgaChannel* channel, const FgaRequest* request)
{
    FgaChannelSlot* slot;
    FgaChannelSlotIndex index;

    LWLockAcquire(channel->lock, LW_EXCLUSIVE);

    /* 1) slot만 freelist에서 가져오기 */
    slot = acquire_slot(channel->pool);
    if (slot == NULL)
    {
        LWLockRelease(channel->lock);
        return NULL;
    }

    index = (slot - channel->pool->slots);
    Assert(index < channel->pool->size);

    /* 2) slot 내용 먼저 다 쓰기 */
    slot->backend_pid = MyProcPid;
    slot->request_id = pg_atomic_add_fetch_u64(&channel->request_id, 1);
    slot->request = *request;

    if (!queue_enqueue(channel->queue, index))
    {
        /* 롤백 처리 */
        release_slot(channel->pool, slot);
        LWLockRelease(channel->lock);
        return NULL;
    }

    LWLockRelease(channel->lock);
    return slot;
}

static FgaChannelSlotState wait_response(FgaChannel* channel, FgaChannelSlot* slot)
{
    FgaChannelSlotState state;

    Assert(channel != NULL);
    Assert(slot != NULL);
    Assert(slot->request_id != 0);
    Assert(slot->backend_pid == MyProcPid);

    PG_TRY();
    {
        for (;;)
        {
            state = (FgaChannelSlotState)pg_atomic_read_u32(&slot->state);
            if (state == FGA_CHANNEL_SLOT_DONE || state == FGA_CHANNEL_SLOT_ERROR || state == FGA_CHANNEL_SLOT_CANCELED)
                break;

            int rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, -1, PG_WAIT_EXTENSION);

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
        else if (cur == FGA_CHANNEL_SLOT_DONE || cur == FGA_CHANNEL_SLOT_ERROR)
        {
            /* 이미 처리 완료된 상태였으면 여기서 정리해도 됨 */
            postfga_channel_release_slot(channel, slot);
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
void postfga_channel_release_slot(FgaChannel* channel, FgaChannelSlot* slot)
{
    LWLockAcquire(channel->lock, LW_EXCLUSIVE);
    release_slot(channel->pool, slot);
    LWLockRelease(channel->lock);
}

uint16 postfga_channel_drain_slots(FgaChannel* channel, uint16 max_count, FgaChannelSlot** out_slots)
{
    uint16 count;
    uint16 buf[MAX_DRAIN];

    /* 요청이 너무 크면 상한으로 잘라버림 */
    if (max_count > MAX_DRAIN)
        max_count = MAX_DRAIN;

    LWLockAcquire(channel->lock, LW_EXCLUSIVE);
    count = queue_drain(channel->queue, max_count, buf);
    LWLockRelease(channel->lock);

    for (uint16 i = 0; i < count; ++i)
    {
        out_slots[i] = &channel->pool->slots[buf[i]];
    }

    return count;
}

void postfga_channel_execute(const FgaRequest* request, FgaResponse* response)
{
    PostfgaShmemState* state = postfga_get_shmem_state();
    FgaChannel* channel = state->channel;
    FgaChannelSlot* slot = write_request(channel, request);
    FgaChannelSlotState slot_state;

    if (slot == NULL)
    {
        ereport(ERROR, errmsg("postfga: failed to enqueue check request"));
    }

    /* BGW 깨우기 */
    SetLatch(state->bgw_latch);

    slot_state = wait_response(channel, slot);

    if (slot_state == FGA_CHANNEL_SLOT_CANCELED)
    {
        /* 여기까지 왔으면 논리적으로는 이미 위에서 ERROR가 났을 확률이 높지만,
         * 방어 코드로 이렇게 한 번 더 정리해줄 수 있음.
         */
        postfga_channel_release_slot(channel, slot);
        ereport(ERROR, errmsg("postfga: request was canceled"));
    }

    pg_read_barrier();
    memcpy(response, &slot->response, sizeof(FgaResponse));

    postfga_channel_release_slot(channel, slot);
}

bool postfga_channel_check(const char* object_type,
                           const char* object_id,
                           const char* subject_type,
                           const char* subject_id,
                           const char* relation)
{
    FgaRequest request;
    FgaResponse response;
    MemSet(&request, 0, sizeof(request));
    request.type = FGA_REQUEST_CHECK_TUPLE;
    strncpy(request.body.checkTuple.store_id, "default", sizeof(request.body.checkTuple.store_id) - 1);
    strncpy(
        request.body.checkTuple.tuple.object_type, object_type, sizeof(request.body.checkTuple.tuple.object_type) - 1);
    strncpy(request.body.checkTuple.tuple.object_id, object_id, sizeof(request.body.checkTuple.tuple.object_id) - 1);
    strncpy(request.body.checkTuple.tuple.relation, relation, sizeof(request.body.checkTuple.tuple.relation) - 1);
    strncpy(request.body.checkTuple.tuple.subject_type,
            subject_type,
            sizeof(request.body.checkTuple.tuple.subject_type) - 1);
    strncpy(request.body.checkTuple.tuple.subject_id, subject_id, sizeof(request.body.checkTuple.tuple.subject_id) - 1);

    postfga_channel_execute(&request, &response);
    return response.body.checkTuple.allow;
}
