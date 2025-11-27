#ifndef POSTFGA_CHECK_SLOT_H
#define POSTFGA_CHECK_SLOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>
#include <miscadmin.h>
#include <port/atomics.h>
#include <lib/ilist.h>

#include "fga_type.h"

    typedef uint16_t FgaChannelSlotIndex;

    typedef enum FgaChannelSlotState
    {
        FGA_CHECK_SLOT_EMPTY = 0,
        FGA_CHECK_SLOT_PENDING,
        FGA_CHECK_SLOT_PROCESSING,
        FGA_CHECK_SLOT_DONE,
        FGA_CHECK_SLOT_ERROR
    } FgaChannelSlotState;

    typedef struct FgaChannelSlot
    {
        slist_node node;        /* 내부 연결 리스트 용도 */
        pg_atomic_uint32 state; /* FgaChannelSlotState */
        pid_t backend_pid;      /* 요청한 백엔드 PID */
        uint64 request_id;      /* 요청 식별자 */
        FgaRequest request;     /* 요청 내용 */
        bool allowed;           /* 체크 결과 */
        int32 error_code;       /* 에러 코드 (정상 시 0) */
    } FgaChannelSlot;

    typedef struct FgaChannelSlotPool
    {
        slist_head head;
        FgaChannelSlot slots[FLEXIBLE_ARRAY_MEMBER];
    } FgaChannelSlotPool;

    static void
    pool_init(FgaChannelSlotPool *pool, uint32 max_slots)
    {
        uint32 i;

        slist_init(&pool->head);

        for (i = 0; i < max_slots; i++)
        {
            FgaChannelSlot *slot = &pool->slots[i];

            slist_push_head(&pool->head, &slot->node);

            pg_atomic_init_u32(&slot->state, FGA_CHECK_SLOT_EMPTY);
            slot->backend_pid = InvalidPid;
            slot->request_id = 0;
            MemSet(&slot->request, 0, sizeof(FgaRequest));
            slot->allowed = false;
            slot->error_code = 0;
        }
    }

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

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHECK_SLOT_H */