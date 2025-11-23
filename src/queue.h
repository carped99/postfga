#ifndef POSTFGA_QUEUE_H
#define POSTFGA_QUEUE_H

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t
#include <stdbool.h> // bool
#include <assert.h>

#include "request.h" // RequestPayload 정의

#ifdef __cplusplus
extern "C"
{
#endif

    typedef uint32_t postfga_qsize_t;

/*
 * buffer_size 가 2의 거듭제곱인지 확인하는 매크로
 */
#define POSTFGA_IS_POWER_OF_TWO(sz) (((sz) & ((sz) - 1u)) == 0u)

    /*
     * PostfgaRequestQueue
     *
     * - RequestPayload 전용 링버퍼
     * - head_index: 다음 enqueue 위치
     * - tail_index: 다음 dequeue 위치
     * - buffer_mask: (capacity - 1), capacity는 반드시 2의 거듭제곱
     *
     * 한 칸 비워두는 패턴을 사용하므로,
     * 실제로 담을 수 있는 최대 요소 개수는 (capacity - 1).
     */
    typedef struct PostfgaRequestQueue
    {
        postfga_qsize_t mask;   /* capacity - 1 (power-of-two 전제) */
        postfga_qsize_t head;   /* enqueue 위치 */
        postfga_qsize_t tail;   /* dequeue 위치 */
        RequestPayload *buffer; /* 요소 배열 (길이 = capacity) */
    } PostfgaRequestQueue;

    /*
     * postfga_init_queue
     *
     * - buffer      : 사전에 할당된 RequestPayload 배열
     * - capacity    : 배열 길이(요소 개수). 반드시 2의 거듭제곱이어야 함.
     * - 실제로 저장 가능한 요소 최대 개수는 (capacity - 1).
     */
    static inline void
    postfga_init_queue(PostfgaRequestQueue *q,
                       RequestPayload *buffer,
                       postfga_qsize_t capacity)
    {
        assert(q != NULL);
        assert(buffer != NULL);
        assert(capacity > 1u);
        assert(POSTFGA_IS_POWER_OF_TWO(capacity));

        q->buffer = buffer;
        q->mask = capacity - 1u;
        q->head = 0u;
        q->tail = 0u;
    }

    /*
     * 현재 요소 개수 반환
     */
    static inline postfga_qsize_t
    postfga_queue_size(const PostfgaRequestQueue *q)
    {
        return (q->head - q->tail) & q->mask;
    }

    /*
     * 큐가 비었는지 확인
     */
    static inline bool
    postfga_queue_is_empty(const PostfgaRequestQueue *q)
    {
        return q->head == q->tail;
    }

    /*
     * 큐가 가득 찼는지 확인
     * - 한 칸 비워두는 패턴: (head - tail) & mask == mask 이면 full
     */
    static inline bool
    postfga_queue_is_full(const PostfgaRequestQueue *q)
    {
        return postfga_queue_size(q) == q->mask;
    }

    /*
     * 남은 공간(더 넣을 수 있는 요소 개수) 반환
     * 최대값은 (capacity - 1)
     */
    static inline postfga_qsize_t
    postfga_queue_available(const PostfgaRequestQueue *q)
    {
        postfga_qsize_t capacity = q->mask + 1u;
        return (capacity - 1u) - postfga_queue_size(q);
    }

    /*
     * enqueue 하나
     *
     * - 성공 시 true, 큐가 가득 차 있으면 false
     * - 동시성 제어(LWLock 등)는 호출자가 담당
     */
    static inline bool
    postfga_enqueue_request(PostfgaRequestQueue *q, const RequestPayload *item)
    {
        postfga_qsize_t idx;

        if (postfga_queue_is_full(q))
            return false;

        idx = q->head & q->mask;
        q->buffer[idx] = *item; /* struct 복사 */

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
    postfga_dequeue_request(PostfgaRequestQueue *q, RequestPayload *out_item)
    {
        postfga_qsize_t idx;
        if (postfga_queue_is_empty(q))
            return false;

        idx = q->tail & q->mask;
        *out_item = q->buffer[idx];
        q->tail++;

        return true;
    }

    /*
     * batch dequeue
     *
     * - 최대 max_count 개까지 꺼내서 out 배열에 채움
     * - 실제 꺼낸 개수 반환
     */
    static inline postfga_qsize_t
    postfga_dequeue_requests(PostfgaRequestQueue *q, RequestPayload *out, postfga_qsize_t max_count)
    {
        postfga_qsize_t n = 0;
        postfga_qsize_t mask = q->mask;
        postfga_qsize_t idx;

        while (n < max_count && !postfga_queue_is_empty(q))
        {
            idx = q->tail & mask;
            out[n] = q->buffer[idx];
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
    postfga_queue_peek(const PostfgaRequestQueue *q, postfga_qsize_t index, RequestPayload *out_item)
    {
        postfga_qsize_t idx;
        postfga_qsize_t size = postfga_queue_size(q);
        if (index >= size)
            return false;

        idx = (q->tail + index) & q->mask;

        *out_item = q->buffer[idx];
        return true;
    }

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_QUEUE_H */
