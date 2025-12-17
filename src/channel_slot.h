#ifndef FGA_CHANNEL_SLOT_H
#define FGA_CHANNEL_SLOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

#include <lib/ilist.h>
#include <miscadmin.h>
#include <port/atomics.h>

#include "channel.h"


    static void pool_init(FgaChannelSlotPool* pool, uint32 max_slots)
    {
        uint32 i;

        slist_init(&pool->head);

        for (i = 0; i < max_slots; i++)
        {
            FgaChannelSlot* slot = &pool->slots[i];

            pg_atomic_init_u32(&slot->state, FGA_CHANNEL_SLOT_EMPTY);
            slot->backend_pid = InvalidPid;
            MemSet(&slot->payload, 0, sizeof(slot->payload));

            slist_push_head(&pool->head, &slot->node);
        }
    }

    /*
     * queue_init
     *
     * - buffer      : 사전에 할당된 RequestPayload 배열
     * - capacity    : 배열 길이(요소 개수). 반드시 2의 거듭제곱이어야 함.
     * - 실제로 저장 가능한 요소 최대 개수는 (capacity - 1).
     */
    static void queue_init(FgaChannelSlotQueue* q, uint16_t capacity)
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
    static inline uint32 queue_drain(FgaChannelSlotQueue* q, uint32 max_count, uint32* out_values)
    {
        uint32 n = 0;
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
     * - index 범위: 0 <= index < fga_queue_size(q)
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

#ifdef __cplusplus
}
#endif

#endif /* FGA_CHANNEL_SLOT_H */
