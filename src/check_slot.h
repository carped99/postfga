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

    typedef uint16_t FgaCheckSlotIndex;

    typedef enum FgaCheckSlotState
    {
        FGA_CHECK_SLOT_EMPTY = 0,
        FGA_CHECK_SLOT_PENDING,
        FGA_CHECK_SLOT_PROCESSING,
        FGA_CHECK_SLOT_DONE,
        FGA_CHECK_SLOT_ERROR
    } FgaCheckSlotState;

    typedef struct FgaCheckSlot
    {
        slist_node node;              /* 내부 연결 리스트 용도 */
        pg_atomic_uint32 state;       /* FgaCheckSlotState */
        pid_t backend_pid;            /* 요청한 백엔드 PID */
        uint64 request_id;            /* 요청 식별자 */
        FgaCheckTupleRequest request; /* 요청 내용 */
        bool allowed;                 /* 체크 결과 */
        int32 error_code;             /* 에러 코드 (정상 시 0) */
    } FgaCheckSlot;

    typedef struct FgaCheckSlotPool
    {
        slist_head head;
        FgaCheckSlot slots[FLEXIBLE_ARRAY_MEMBER];
    } FgaCheckSlotPool;

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

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHECK_SLOT_H */