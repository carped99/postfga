#ifndef POSTFGA_CHECK_SLOT_H
#define POSTFGA_CHECK_SLOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

#include <lib/ilist.h>
#include <miscadmin.h>
#include <port/atomics.h>

#include "request.h"

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
        FgaResponse response;   /* 응답 내용 */
    } FgaChannelSlot;

    typedef struct FgaChannelSlotPool
    {
        slist_head head;
        FgaChannelSlot slots[FLEXIBLE_ARRAY_MEMBER];
    } FgaChannelSlotPool;

    static void pool_init(FgaChannelSlotPool* pool, uint32 max_slots)
    {
        uint32 i;

        slist_init(&pool->head);

        for (i = 0; i < max_slots; i++)
        {
            FgaChannelSlot* slot = &pool->slots[i];

            slist_push_head(&pool->head, &slot->node);

            pg_atomic_init_u32(&slot->state, FGA_CHECK_SLOT_EMPTY);
            slot->backend_pid = InvalidPid;
            slot->request_id = 0;
            MemSet(&slot->request, 0, sizeof(FgaRequest));
            MemSet(&slot->response, 0, sizeof(FgaResponse));
        }
    }

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHECK_SLOT_H */
