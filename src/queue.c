/*-------------------------------------------------------------------------
 *
 * queue.c
 *    Thread-safe ring buffer queue for gRPC requests in shared memory.
 *
 * This module implements:
 *   - Process-safe enqueue/dequeue operations using LWLock + atomic operations
 *   - Ring buffer for efficient memory usage
 *   - Background worker notification via latch
 *   - Request result tracking
 *
 * Ring Buffer Design:
 *   - Fixed-size circular buffer in shared memory
 *   - Protected by LWLock for multi-process safety
 *   - Atomic operations for additional safety and memory ordering
 *   - Single producer pattern (multiple FDW backends serialized by lock)
 *   - Single consumer (BGW)
 *   - Full/empty detection using atomic size counter
 *
 * Multi-Process Safety:
 *   PostgreSQL uses multi-process (not multi-thread) architecture.
 *   All queue modifications are protected by LWLock to prevent race conditions.
 *   Atomic operations provide memory barriers and ensure correct ordering.
 *   This is NOT a lock-free data structure - LWLock is essential for safety.
 *
 *-------------------------------------------------------------------------
 */

#include <postgres.h>

#include <storage/latch.h>
#include <storage/lwlock.h>
#include <utils/timestamp.h>
#include <port/atomics.h>
#include <miscadmin.h>

#include <string.h>

#include "queue.h"
#include "state.h"
#include "stats.h"

/* -------------------------------------------------------------------------
 * Private Helper Functions
 * -------------------------------------------------------------------------
 */

/*
 * get_next_request_id
 *
 * Generate a unique request ID (not thread-safe, assumes caller holds lock)
 */
static inline uint32
get_next_request_id(PostfgaShmemState *state)
{
    static uint32 counter = 0;

    /* Use simple counter for now, could use atomic or timestamp-based ID */
    return ++counter;
}

/*
 * queue_is_full
 *
 * Check if the queue is full
 *
 * Returns: true if queue is full, false otherwise
 */
static inline bool
queue_is_full(PostfgaShmemState *state)
{
    uint32 size = pg_atomic_read_u32(&state->queue_size);
    return size >= MAX_PENDING_REQ;
}

/*
 * queue_is_empty
 *
 * Check if the queue is empty
 *
 * Returns: true if queue is empty, false otherwise
 */
static inline bool
queue_is_empty(PostfgaShmemState *state)
{
    uint32 size = pg_atomic_read_u32(&state->queue_size);
    return size == 0;
}

/*
 * queue_size
 *
 * Get current number of items in queue
 *
 * Returns: Number of items in queue
 */
static inline uint32
queue_size(PostfgaShmemState *state)
{
    return pg_atomic_read_u32(&state->queue_size);
}

/* -------------------------------------------------------------------------
 * Public Queue Operations
 * -------------------------------------------------------------------------
 */

/*
 * enqueue_grpc_request
 *
 * Enqueue a gRPC request for background processing.
 *
 * Args:
 *   object_type   - Object type (e.g., "document")
 *   object_id     - Object ID (e.g., "123")
 *   subject_type  - Subject type (e.g., "user")
 *   subject_id    - Subject ID (e.g., "alice")
 *   relation      - Relation name (e.g., "read")
 *
 * Returns: Request ID (0 if queue is full)
 *
 * Thread safety: Uses atomic operations and LWLock
 */
uint32
enqueue_grpc_request(const char *object_type, const char *object_id,
                     const char *subject_type, const char *subject_id,
                     const char *relation)
{
    PostfgaShmemState *state;
    GrpcRequest *req;
    uint32 tail;
    uint32 request_id = 0;

    /* Validate input */
    if (!object_type || !object_id || !subject_type || !subject_id || !relation)
    {
        elog(WARNING, "PostFGA: Invalid parameters for enqueue_grpc_request");
        return 0;
    }

    /* Get shared state */
    state = get_shared_state();
    if (!state || !state->request_queue)
    {
        elog(WARNING, "PostFGA: Queue not initialized");
        return 0;
    }

    /* Acquire lock for queue modification */
    LWLockAcquire(state->lock, LW_EXCLUSIVE);

    /* Check if queue is full */
    if (queue_is_full(state))
    {
        LWLockRelease(state->lock);
        elog(WARNING, "PostFGA: Request queue is full (%u/%u)",
             queue_size(state), MAX_PENDING_REQ);
        /* TODO: Add queue_full stats counter if needed */
        return 0;
    }

    /* Get current tail position */
    tail = pg_atomic_read_u32(&state->queue_tail);

    /* Get pointer to queue entry */
    RequestPayload *payload = &state->request_queue[tail];
    req = &payload->check;  /* Use check request type */

    /* Initialize request */
    memset(payload, 0, sizeof(RequestPayload));

    request_id = get_next_request_id(state);
    req->base.request_id = request_id;
    req->base.type = REQ_TYPE_CHECK;  /* Set request type */
    req->base.status = REQ_STATUS_PENDING;
    req->base.backend_pid = MyProcPid;
    req->base.backend_id = MyBackendType;
    req->base.backend_latch = MyLatch;  /* Store backend latch for wakeup */

    /* Copy strings (with bounds checking) */
    strncpy(req->object_type, object_type, NAME_MAX_LEN - 1);
    req->object_type[NAME_MAX_LEN - 1] = '\0';

    strncpy(req->object_id, object_id, NAME_MAX_LEN - 1);
    req->object_id[NAME_MAX_LEN - 1] = '\0';

    strncpy(req->subject_type, subject_type, NAME_MAX_LEN - 1);
    req->subject_type[NAME_MAX_LEN - 1] = '\0';

    strncpy(req->subject_id, subject_id, NAME_MAX_LEN - 1);
    req->subject_id[NAME_MAX_LEN - 1] = '\0';

    strncpy(req->relation, relation, RELATION_MAX_LEN - 1);
    req->relation[RELATION_MAX_LEN - 1] = '\0';

    /* Set timestamp */
    req->base.created_at = GetCurrentTimestamp();

    /* Initialize result fields */
    req->allowed = false;
    req->base.error_code = 0;
    req->base.success = false;

    /* Advance tail pointer (circular) */
    tail = (tail + 1) % MAX_PENDING_REQ;
    pg_atomic_write_u32(&state->queue_tail, tail);

    /* Increment size counter */
    pg_atomic_fetch_add_u32(&state->queue_size, 1);

    /* Update statistics */
    stats_inc_request_enqueued(&state->stats);

    /* Release lock */
    LWLockRelease(state->lock);

    elog(DEBUG2, "PostFGA: Enqueued request %u (queue size: %u/%u)",
         request_id, queue_size(state), MAX_PENDING_REQ);

    /* Notify background worker */
    notify_bgw_of_pending_work();

    return request_id;
}

/*
 * enqueue_write_request
 *
 * Enqueue a Write tuple request for background processing.
 *
 * Args:
 *   object_type   - Object type (e.g., "document")
 *   object_id     - Object ID (e.g., "123")
 *   subject_type  - Subject type (e.g., "user")
 *   subject_id    - Subject ID (e.g., "alice")
 *   relation      - Relation name (e.g., "reader")
 *
 * Returns: Request ID (0 if queue is full)
 */
uint32
enqueue_write_request(const char *object_type, const char *object_id,
                     const char *subject_type, const char *subject_id,
                     const char *relation)
{
    PostfgaShmemState *state;
    WriteRequest *req;
    uint32 tail;
    uint32 request_id = 0;

    /* Validate input */
    if (!object_type || !object_id || !subject_type || !subject_id || !relation)
    {
        elog(WARNING, "PostFGA: Invalid parameters for enqueue_write_request");
        return 0;
    }

    /* Get shared state */
    state = get_shared_state();
    if (!state || !state->request_queue)
    {
        elog(WARNING, "PostFGA: Queue not initialized");
        return 0;
    }

    /* Acquire lock for queue modification */
    LWLockAcquire(state->lock, LW_EXCLUSIVE);

    /* Check if queue is full */
    if (queue_is_full(state))
    {
        LWLockRelease(state->lock);
        elog(WARNING, "PostFGA: Request queue is full (%u/%u)",
             queue_size(state), MAX_PENDING_REQ);
        return 0;
    }

    /* Get current tail position */
    tail = pg_atomic_read_u32(&state->queue_tail);

    /* Get pointer to queue entry */
    RequestPayload *payload = &state->request_queue[tail];
    req = &payload->write;  /* Use write request type */

    /* Initialize request */
    memset(payload, 0, sizeof(RequestPayload));

    request_id = get_next_request_id(state);
    req->base.request_id = request_id;
    req->base.type = REQ_TYPE_WRITE;  /* Set request type */
    req->base.status = REQ_STATUS_PENDING;
    req->base.backend_pid = MyProcPid;
    req->base.backend_id = MyBackendType;
    req->base.backend_latch = MyLatch;  /* Store backend latch for wakeup */

    /* Copy strings (with bounds checking) */
    strncpy(req->object_type, object_type, NAME_MAX_LEN - 1);
    req->object_type[NAME_MAX_LEN - 1] = '\0';

    strncpy(req->object_id, object_id, NAME_MAX_LEN - 1);
    req->object_id[NAME_MAX_LEN - 1] = '\0';

    strncpy(req->subject_type, subject_type, NAME_MAX_LEN - 1);
    req->subject_type[NAME_MAX_LEN - 1] = '\0';

    strncpy(req->subject_id, subject_id, NAME_MAX_LEN - 1);
    req->subject_id[NAME_MAX_LEN - 1] = '\0';

    strncpy(req->relation, relation, RELATION_MAX_LEN - 1);
    req->relation[RELATION_MAX_LEN - 1] = '\0';

    /* Set timestamp */
    req->base.created_at = GetCurrentTimestamp();

    /* Initialize result fields */
    req->base.error_code = 0;
    req->base.success = false;

    /* Advance tail pointer (circular) */
    tail = (tail + 1) % MAX_PENDING_REQ;
    pg_atomic_write_u32(&state->queue_tail, tail);

    /* Increment size counter */
    pg_atomic_fetch_add_u32(&state->queue_size, 1);

    /* Update statistics */
    stats_inc_request_enqueued(&state->stats);

    /* Release lock */
    LWLockRelease(state->lock);

    elog(DEBUG2, "PostFGA: Enqueued write request %u (queue size: %u/%u)",
         request_id, queue_size(state), MAX_PENDING_REQ);

    /* Notify background worker */
    notify_bgw_of_pending_work();

    return request_id;
}

/*
 * dequeue_grpc_requests
 *
 * Dequeue gRPC requests for processing.
 *
 * Args:
 *   requests  - Buffer to store dequeued requests
 *   count     - In: max requests to dequeue, Out: actual count dequeued
 *
 * Returns: true if any requests were dequeued, false otherwise
 *
 * Thread safety: Uses atomic operations and LWLock
 *
 * Note: This is designed for single consumer (BGW). For multiple consumers,
 *       additional synchronization would be needed.
 */
bool
dequeue_grpc_requests(GrpcRequest *requests, uint32 *count)
{
    PostfgaShmemState *state;
    uint32 head;
    uint32 max_count;
    uint32 dequeued = 0;
    uint32 i;

    /* Validate input */
    if (!requests || !count || *count == 0)
    {
        elog(WARNING, "PostFGA: Invalid parameters for dequeue_grpc_requests");
        return false;
    }

    max_count = *count;
    *count = 0;

    /* Get shared state */
    state = get_shared_state();
    if (!state || !state->request_queue)
    {
        elog(WARNING, "PostFGA: Queue not initialized");
        return false;
    }

    /* Acquire lock for queue modification */
    LWLockAcquire(state->lock, LW_EXCLUSIVE);

    /* Check if queue is empty */
    if (queue_is_empty(state))
    {
        LWLockRelease(state->lock);
        return false;
    }

    /* Get current head position */
    head = pg_atomic_read_u32(&state->queue_head);

    /* Dequeue up to max_count requests */
    for (i = 0; i < max_count && !queue_is_empty(state); i++)
    {
        /* Copy request to output buffer (only CHECK requests for now) */
        RequestPayload *payload = &state->request_queue[head];

        /* For backward compatibility, only copy check requests */
        if (payload->base.type == REQ_TYPE_CHECK)
        {
            memcpy(&requests[i], &payload->check, sizeof(GrpcRequest));
        }
        else
        {
            elog(WARNING, "PostFGA: Unsupported request type %d", payload->base.type);
            /* Skip this request for now */
            head = (head + 1) % MAX_PENDING_REQ;
            pg_atomic_write_u32(&state->queue_head, head);
            pg_atomic_fetch_sub_u32(&state->queue_size, 1);
            continue;
        }

        /* Advance head pointer (circular) */
        head = (head + 1) % MAX_PENDING_REQ;
        pg_atomic_write_u32(&state->queue_head, head);

        /* Decrement size counter */
        pg_atomic_fetch_sub_u32(&state->queue_size, 1);

        dequeued++;

        /* Update statistics */
        stats_inc_request_processed(&state->stats);
    }

    /* Release lock */
    LWLockRelease(state->lock);

    *count = dequeued;

    elog(DEBUG2, "PostFGA: Dequeued %u requests (queue size: %u/%u)",
         dequeued, queue_size(state), MAX_PENDING_REQ);

    return (dequeued > 0);
}

/*
 * dequeue_requests
 *
 * Dequeue requests as RequestPayload (supports all request types).
 *
 * Args:
 *   requests  - Buffer to store dequeued requests
 *   count     - In: max requests to dequeue, Out: actual count dequeued
 *
 * Returns: true if any requests were dequeued, false otherwise
 */
bool
dequeue_requests(RequestPayload *requests, uint32 *count)
{
    PostfgaShmemState *state;
    uint32 head;
    uint32 max_count;
    uint32 dequeued = 0;
    uint32 i;

    /* Validate input */
    if (!requests || !count || *count == 0)
    {
        elog(WARNING, "PostFGA: Invalid parameters for dequeue_requests");
        return false;
    }

    max_count = *count;
    *count = 0;

    /* Get shared state */
    state = get_shared_state();
    if (!state || !state->request_queue)
    {
        elog(WARNING, "PostFGA: Queue not initialized");
        return false;
    }

    /* Acquire lock for queue modification */
    LWLockAcquire(state->lock, LW_EXCLUSIVE);

    /* Check if queue is empty */
    if (queue_is_empty(state))
    {
        LWLockRelease(state->lock);
        return false;
    }

    /* Get current head position */
    head = pg_atomic_read_u32(&state->queue_head);

    /* Dequeue up to max_count requests */
    for (i = 0; i < max_count && !queue_is_empty(state); i++)
    {
        /* Copy request to output buffer */
        RequestPayload *payload = &state->request_queue[head];
        memcpy(&requests[i], payload, sizeof(RequestPayload));

        /* Advance head pointer (circular) */
        head = (head + 1) % MAX_PENDING_REQ;
        pg_atomic_write_u32(&state->queue_head, head);

        /* Decrement size counter */
        pg_atomic_fetch_sub_u32(&state->queue_size, 1);

        dequeued++;

        /* Update statistics */
        stats_inc_request_processed(&state->stats);
    }

    /* Release lock */
    LWLockRelease(state->lock);

    *count = dequeued;

    elog(DEBUG2, "PostFGA: Dequeued %u requests (queue size: %u/%u)",
         dequeued, queue_size(state), MAX_PENDING_REQ);

    return (dequeued > 0);
}

/*
 * wait_for_grpc_result
 *
 * Wait for gRPC result (blocking).
 *
 * Args:
 *   request_id  - Request ID to wait for
 *   allowed     - Output: whether access is allowed
 *   error_code  - Output: error code (0 = success)
 *
 * Returns: true if result received, false on timeout/error
 *
 * Implementation Note:
 *   This is a simplified implementation. In production, you would:
 *   1. Use a condition variable or latch to wait efficiently
 *   2. Store results in a separate hash table indexed by request_id
 *   3. Implement timeout handling
 *   4. Handle backend termination gracefully
 *
 * Current implementation:
 *   - Polls the queue for completed requests (inefficient but simple)
 *   - Timeout after a fixed duration
 */
bool
wait_for_grpc_result(uint32 request_id, bool *allowed, uint32 *error_code)
{
    PostfgaShmemState *state;
    int max_wait_ms = 5000;  /* 5 seconds */
    int wait_interval_ms = 10;  /* 10 ms per iteration */
    int iterations = max_wait_ms / wait_interval_ms;
    int i;

    /* Validate input */
    if (request_id == 0 || !allowed || !error_code)
    {
        elog(WARNING, "PostFGA: Invalid parameters for wait_for_grpc_result");
        return false;
    }

    /* Get shared state */
    state = get_shared_state();
    if (!state)
    {
        elog(WARNING, "PostFGA: Shared state not initialized");
        return false;
    }

    /* Poll for result */
    for (i = 0; i < iterations; i++)
    {
        CHECK_FOR_INTERRUPTS();

        /* Search queue for matching request_id with completed status */
        LWLockAcquire(state->lock, LW_SHARED);

        /* Linear search through queue (inefficient but simple) */
        for (uint32 j = 0; j < MAX_PENDING_REQ; j++)
        {
            RequestPayload *payload = &state->request_queue[j];
            BaseRequest *base = &payload->base;

            if (base->request_id == request_id)
            {
                if (base->status == REQ_STATUS_COMPLETED)
                {
                    /* For CHECK requests, extract allowed field */
                    if (base->type == REQ_TYPE_CHECK)
                    {
                        *allowed = payload->check.allowed;
                    }
                    else
                    {
                        *allowed = base->success;
                    }
                    *error_code = base->error_code;
                    LWLockRelease(state->lock);

                    elog(DEBUG2, "PostFGA: Result received for request %u: allowed=%d",
                         request_id, *allowed);
                    return true;
                }
                else if (base->status == REQ_STATUS_ERROR)
                {
                    *allowed = false;
                    *error_code = base->error_code;
                    LWLockRelease(state->lock);

                    elog(WARNING, "PostFGA: Request %u failed with error %u",
                         request_id, *error_code);
                    return false;
                }
                break;
            }
        }

        LWLockRelease(state->lock);

        /* Sleep for a short interval */
        pg_usleep(wait_interval_ms * 1000L);  /* Convert ms to microseconds */
    }

    /* Timeout */
    elog(WARNING, "PostFGA: Timeout waiting for request %u result", request_id);
    /* TODO: Add queue_timeout stats counter if needed */
    return false;
}

/*
 * set_grpc_result
 *
 * Set gRPC result for a request (called by BGW after processing).
 *
 * Args:
 *   request_id  - Request ID
 *   allowed     - Whether access is allowed
 *   error_code  - Error code (0 = success)
 *
 * Thread safety: Uses LWLock
 *
 * Note: In production, this should use a separate result hash table
 *       indexed by request_id for O(1) lookup instead of linear search.
 */
void
set_grpc_result(uint32 request_id, bool allowed, uint32 error_code)
{
    PostfgaShmemState *state;
    bool found = false;

    /* Validate input */
    if (request_id == 0)
    {
        elog(WARNING, "PostFGA: Invalid request_id for set_grpc_result");
        return;
    }

    /* Get shared state */
    state = get_shared_state();
    if (!state || !state->request_queue)
    {
        elog(WARNING, "PostFGA: Queue not initialized");
        return;
    }

    /* Acquire lock */
    LWLockAcquire(state->lock, LW_EXCLUSIVE);

    /* Linear search for matching request_id */
    for (uint32 i = 0; i < MAX_PENDING_REQ; i++)
    {
        RequestPayload *payload = &state->request_queue[i];
        BaseRequest *base = &payload->base;

        if (base->request_id == request_id && base->status == REQ_STATUS_PENDING)
        {
            /* Update result based on request type */
            if (base->type == REQ_TYPE_CHECK)
            {
                payload->check.allowed = allowed;
            }

            base->success = allowed;
            base->error_code = error_code;
            base->status = (error_code == 0) ? REQ_STATUS_COMPLETED : REQ_STATUS_ERROR;
            base->completed_at = GetCurrentTimestamp();

            /* Wake up waiting backend if latch is set */
            if (base->backend_latch != NULL)
            {
                SetLatch(base->backend_latch);
            }

            found = true;
            break;
        }
    }

    /* Release lock */
    LWLockRelease(state->lock);

    if (found)
    {
        elog(DEBUG2, "PostFGA: Set result for request %u: allowed=%d, error=%u",
             request_id, allowed, error_code);
    }
    else
    {
        elog(WARNING, "PostFGA: Request %u not found in queue", request_id);
    }
}

/*
 * notify_bgw_of_pending_work
 *
 * Notify background worker that there is pending work in the queue.
 *
 * Thread safety: Read-only access to latch pointer (set once at startup)
 */
void
notify_bgw_of_pending_work(void)
{
    PostfgaShmemState *state;

    state = get_shared_state();
    if (!state)
    {
        elog(DEBUG2, "PostFGA: Cannot notify BGW, shared state not initialized");
        return;
    }

    /* Check if BGW has registered its latch */
    if (state->bgw_latch)
    {
        SetLatch(state->bgw_latch);
        stats_inc_bgw_wakeup(&state->stats);
        elog(DEBUG2, "PostFGA: Notified BGW of pending work");
    }
    else
    {
        elog(DEBUG2, "PostFGA: BGW latch not registered, cannot notify");
    }
}

/* -------------------------------------------------------------------------
 * Queue Management Functions
 * -------------------------------------------------------------------------
 */

/*
 * clear_completed_requests
 *
 * Clear completed/error requests from the queue to make room for new ones.
 *
 * This is a maintenance function that can be called periodically by BGW
 * to reclaim space in the queue.
 *
 * Returns: Number of requests cleared
 */
uint32
clear_completed_requests(void)
{
    PostfgaShmemState *state;
    uint32 cleared = 0;

    state = get_shared_state();
    if (!state || !state->request_queue)
    {
        return 0;
    }

    LWLockAcquire(state->lock, LW_EXCLUSIVE);

    /* Scan queue and clear completed/error entries */
    for (uint32 i = 0; i < MAX_PENDING_REQ; i++)
    {
        RequestPayload *payload = &state->request_queue[i];
        BaseRequest *base = &payload->base;

        if (base->status == REQ_STATUS_COMPLETED || base->status == REQ_STATUS_ERROR)
        {
            /* Check if result is old enough to clear (e.g., > 60 seconds) */
            TimestampTz now = GetCurrentTimestamp();
            long secs;
            int microsecs;

            TimestampDifference(base->created_at, now, &secs, &microsecs);

            if (secs > 60)  /* 60 seconds threshold */
            {
                memset(payload, 0, sizeof(RequestPayload));
                cleared++;
            }
        }
    }

    LWLockRelease(state->lock);

    if (cleared > 0)
    {
        elog(DEBUG1, "PostFGA: Cleared %u completed requests from queue", cleared);
    }

    return cleared;
}

/*
 * get_queue_stats
 *
 * Get current queue statistics.
 *
 * Args:
 *   size      - Output: current queue size
 *   capacity  - Output: queue capacity
 *
 * Returns: true if stats retrieved successfully
 */
bool
get_queue_stats(uint32 *size, uint32 *capacity)
{
    PostfgaShmemState *state;

    if (!size || !capacity)
        return false;

    state = get_shared_state();
    if (!state)
        return false;

    *size = queue_size(state);
    *capacity = MAX_PENDING_REQ;

    return true;
}
