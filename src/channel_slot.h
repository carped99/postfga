#ifndef POSTFGA_CHANNEL_SLOT_H
#define POSTFGA_CHANNEL_SLOT_H

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
        FGA_CHANNEL_SLOT_EMPTY = 0,
        FGA_CHANNEL_SLOT_CANCELED,
        FGA_CHANNEL_SLOT_PENDING,
        FGA_CHANNEL_SLOT_PROCESSING,
        FGA_CHANNEL_SLOT_DONE,
        FGA_CHANNEL_SLOT_ERROR
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

    typedef struct FgaChannelSlotQueue
    {
        uint16_t mask;                          /* capacity - 1 (2^n - 1) */
        uint16_t head;                          /* enqueue index */
        uint16_t tail;                          /* dequeue index */
        uint16_t values[FLEXIBLE_ARRAY_MEMBER]; /* [capacity = mask + 1] */
    } FgaChannelSlotQueue;

    static inline void pool_init(FgaChannelSlotPool* pool, uint32 max_slots)
    {
        uint32 i;

        slist_init(&pool->head);

        for (i = 0; i < max_slots; i++)
        {
            FgaChannelSlot* slot = &pool->slots[i];

            slist_push_head(&pool->head, &slot->node);

            pg_atomic_init_u32(&slot->state, FGA_CHANNEL_SLOT_EMPTY);
            slot->backend_pid = InvalidPid;
            slot->request_id = 0;
            MemSet(&slot->request, 0, sizeof(FgaRequest));
            MemSet(&slot->response, 0, sizeof(FgaResponse));
        }
    }


    /*
     * queue_init
     *
     * - buffer      : 사전에 할당된 RequestPayload 배열
     * - capacity    : 배열 길이(요소 개수). 반드시 2의 거듭제곱이어야 함.
     * - 실제로 저장 가능한 요소 최대 개수는 (capacity - 1).
     */
    static inline void queue_init(FgaChannelSlotQueue* q, uint16_t capacity)
    {
        Assert(q != NULL);
        Assert(capacity > 0);
        Assert((capacity & (capacity - 1)) == 0); // 2의 거듭제곱 확인

        q->mask = capacity - 1u;
        q->head = 0u;
        q->tail = 0u;
    }

    static inline uint16_t queue_size(const FgaChannelSlotQueue* q)
    {
        return (q->head - q->tail) & q->mask;
    }

    static inline bool queue_is_empty(const FgaChannelSlotQueue* q)
    {
        return q->head == q->tail;
    }

    static inline bool queue_is_full(const FgaChannelSlotQueue* q)
    {
        return queue_size(q) == q->mask;
    }

    /*
     * 남은 공간(더 넣을 수 있는 요소 개수) 반환
     * 최대값은 (capacity - 1)
     */
    static inline uint16_t queue_available(const FgaChannelSlotQueue* q)
    {
        uint16_t capacity = q->mask + 1u;
        return (capacity - 1u) - queue_size(q);
    }

    /*
     * enqueue 하나
     *
     * - 성공 시 true, 큐가 가득 차 있으면 false
     * - 동시성 제어(LWLock 등)는 호출자가 담당
     */
    static inline bool queue_enqueue(FgaChannelSlotQueue* q, uint16_t slot_index)
    {
        if (queue_is_full(q))
            return false;

        q->values[q->head & q->mask] = slot_index;
        q->head++;

        return true;
    }

    /*
     * dequeue 하나
     *
     * - 성공 시 true, 큐가 비어 있으면 false
     * - 동시성 제어는 호출자가 담당
     */
    static inline bool queue_dequeue(FgaChannelSlotQueue* q, uint16_t* out_slot)
    {
        if (queue_is_empty(q))
            return false;

        *out_slot = q->values[q->tail & q->mask];
        q->tail++;

        return true;
    }

    /*
     * batch dequeue
     *
     * - 최대 max_count 개까지 꺼내서 out 배열에 채움
     * - 실제 꺼낸 개수 반환
     */
    static inline uint16_t queue_drain(FgaChannelSlotQueue* q, uint16_t max_count, uint16_t* out_values)
    {
        uint16_t n = 0;
        while (n < max_count && !queue_is_empty(q))
        {
            out_values[n] = q->values[q->tail & q->mask];
            q->tail++;
            n++;
        }

        return n;
    }

    /*
     * 맨 앞 요소를 제거 없이 조회 (index는 0이 가장 오래된 요소)
     *
     * - index 범위: 0 <= index < postfga_queue_size(q)
     * - 범위 밖이면 false 반환
     */
    static inline bool queue_peek(const FgaChannelSlotQueue* q, uint16_t index, uint16_t* out_slot)
    {
        uint16_t size = queue_size(q);
        if (index >= size)
            return false;

        *out_slot = q->values[(q->tail + index) & q->mask];
        return true;
    }


#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHANNEL_SLOT_H */