#ifndef POSTFGA_CHECK_TUPLE_SLOT_QUEUE_H
#define POSTFGA_CHECK_TUPLE_SLOT_QUEUE_H

#include <postgres.h>

typedef struct FgaChannelSlotQueue
{
    uint16_t mask;                          /* capacity - 1 (2^n - 1) */
    uint16_t head;                          /* enqueue index */
    uint16_t tail;                          /* dequeue index */
    uint16_t values[FLEXIBLE_ARRAY_MEMBER]; /* [capacity = mask + 1] */
} FgaChannelSlotQueue;

/*
 * queue_init
 *
 * - buffer      : 사전에 할당된 RequestPayload 배열
 * - capacity    : 배열 길이(요소 개수). 반드시 2의 거듭제곱이어야 함.
 * - 실제로 저장 가능한 요소 최대 개수는 (capacity - 1).
 */
static inline void
queue_init(FgaChannelSlotQueue *q, uint16_t capacity)
{
    Assert(q != NULL);
    Assert(capacity > 0);
    Assert((capacity & (capacity - 1)) == 0); // 2의 거듭제곱 확인

    q->mask = capacity - 1u;
    q->head = 0u;
    q->tail = 0u;
}

static inline uint16_t
queue_size(const FgaChannelSlotQueue *q)
{
    return (q->head - q->tail) & q->mask;
}

static inline bool
queue_is_empty(const FgaChannelSlotQueue *q)
{
    return q->head == q->tail;
}

static inline bool
queue_is_full(const FgaChannelSlotQueue *q)
{
    return queue_size(q) == q->mask;
}

/*
 * 남은 공간(더 넣을 수 있는 요소 개수) 반환
 * 최대값은 (capacity - 1)
 */
static inline uint16_t
queue_available(const FgaChannelSlotQueue *q)
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
static inline bool
queue_enqueue_slot(FgaChannelSlotQueue *q, uint16_t slot_index)
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
static inline bool
queue_dequeue_slot(FgaChannelSlotQueue *q, uint16_t *out_slot)
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
static inline uint16_t
queue_dequeue_slots(FgaChannelSlotQueue *q, uint16_t *out, uint16_t max_count)
{
    uint16_t n = 0;
    while (n < max_count && !queue_is_empty(q))
    {
        out[n] = q->values[q->tail & q->mask];
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
static inline bool
queue_peek(const FgaChannelSlotQueue *q, uint16_t index, uint16_t *out_slot)
{
    uint16_t size = queue_size(q);
    if (index >= size)
        return false;

    *out_slot = q->values[(q->tail + index) & q->mask];
    return true;
}

#endif /* POSTFGA_CHECK_TUPLE_SLOT_QUEUE_H */