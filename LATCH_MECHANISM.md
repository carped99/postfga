# Latch-based Request-Response Mechanism

## Overview

This document describes the **Latch-based asynchronous request-response mechanism** implemented in the OpenFGA FDW PostgreSQL extension.

## Architecture

### Problem Statement

FDW handlers run in backend processes and cannot directly make blocking gRPC calls without risking PostgreSQL deadlocks. Additionally, we want multiple backends to efficiently share gRPC results through caching.

### Solution: Latch-based Queue

Instead of each FDW directly calling gRPC, we use:

1. **Shared Memory Request Queue**: FDWs enqueue gRPC requests
2. **Background Worker (BGW)**: Processes all pending requests asynchronously
3. **PostgreSQL Latches**: Synchronize FDW and BGW without busy-waiting

## Request-Response Flow

### Timeline

```
FDW Backend                          BGW                 OpenFGA
─────────                            ───                 ────────

1. Check cache
   Cache miss ✗

2. Enqueue request to
   shared memory queue
   request_id = 123

3. Call SetLatch(bgw_latch)
   ─────────────────────────────────→ WaitLatch() wakes up

                                    4. Dequeue all pending
                                       requests from queue

                                    5. Execute gRPC calls
                                       ────────────────────→ Check permissions

                                    ← Return results

                                    6. Write results to
                                       shared memory
                                       status=COMPLETED
                                       allowed=true

                                    7. Call SetLatch(
                                       backend_latch)
   WaitLatch() returns ←────────────────

8. Check shared memory
   for result
   allowed = true

9. Return row to SQL
```

## Code Components

### 1. Shared Memory Structures

**File**: `include/shared_memory.h`

#### `GrpcRequest`
```c
typedef struct {
    uint32 request_id;
    RequestStatus status;
    pid_t backend_pid;
    volatile uint32 *backend_latch;      /* Backend's Latch pointer */

    char object_type[RELATION_NAME_MAX];
    char object_id[RELATION_NAME_MAX];
    char subject_type[RELATION_NAME_MAX];
    char subject_id[RELATION_NAME_MAX];
    char relation[RELATION_NAME_MAX];

    bool allowed;
    uint32 error_code;
    TimestampTz created_at;
} GrpcRequest;
```

#### `AclSharedState`
```c
typedef struct {
    LWLock *lock;
    volatile uint32 *bgw_latch;              /* BGW's Latch pointer */

    GrpcRequest pending_requests[MAX_PENDING_REQUESTS];
    volatile uint32 pending_request_count;
    volatile uint32 next_request_id;

    /* Hash tables for cache and generations */
    HTAB *acl_cache;
    HTAB *relation_bitmap_map;
    HTAB *object_type_gen_map;
    HTAB *object_gen_map;
    HTAB *subject_type_gen_map;
    HTAB *subject_gen_map;
    uint64 next_generation;
} AclSharedState;
```

### 2. FDW Handler (`IterateForeignScan`)

**File**: `src/fdw_handler.c`

#### Step-by-step Process

```c
TupleTableSlot *
openfga_fdw_iterate_scan(ForeignScanState *node)
{
    OpenFGAFdwState *fdw_state = (OpenFGAFdwState *) node->fdw_state;

    /* STEP 1: Check cache first */
    AclCacheKey cache_key;
    AclCacheEntry cache_entry;

    if (cache_lookup(&cache_key, &cache_entry)) {
        if (!is_cache_stale(&cache_entry)) {
            /* Use cached result immediately */
            return build_result_tuple(allowed);
        }
    }

    /* STEP 2: Enqueue gRPC request */
    uint32 request_id = enqueue_grpc_request(
        object_type, object_id,
        subject_type, subject_id,
        relation);

    /* STEP 3: Wake up BGW */
    notify_bgw_of_pending_work();    /* SetLatch(bgw_latch) */

    /* STEP 4: Block until BGW returns result */
    bool allowed;
    uint32 error_code;
    wait_for_grpc_result(request_id, &allowed, &error_code);
    /* This internally calls WaitLatch(MyLatch) */

    /* STEP 5: Return result */
    if (allowed) {
        return build_result_tuple(allowed);
    } else {
        return empty_result();
    }
}
```

### 3. Request Queue API

**File**: `src/shared_memory.c`

#### `enqueue_grpc_request()`
- **Called by**: FDW (backend process)
- **Action**: Add request to shared memory queue
- **Returns**: Unique request ID
- **Synchronization**: Acquires LWLock (exclusive) briefly

```c
uint32 enqueue_grpc_request(
    const char *object_type, const char *object_id,
    const char *subject_type, const char *subject_id,
    const char *relation);
```

#### `notify_bgw_of_pending_work()`
- **Called by**: FDW (after enqueuing)
- **Action**: Signals BGW via `SetLatch(bgw_latch)`
- **Effect**: Wakes up BGW from `WaitLatch()`

```c
void notify_bgw_of_pending_work(void);
```

#### `wait_for_grpc_result()`
- **Called by**: FDW (after enqueuing)
- **Action**: Blocks until BGW completes request
- **Timeout**: 5 seconds
- **Returns**: Success/failure and result (`allowed`, `error_code`)
- **Mechanism**: PostgreSQL `WaitLatch()` with `WL_TIMEOUT`

```c
bool wait_for_grpc_result(
    uint32 request_id,
    bool *allowed,
    uint32 *error_code);
```

### 4. Background Worker (`BgWorkerMain`)

**File**: `src/background_worker.c`

#### Main Loop
```c
void openfga_bgw_main(Datum arg)
{
    AclSharedState *state = get_shared_state();

    /* Register our Latch in shared memory */
    set_bgw_latch(&MyProc->procLatch);

    while (!got_sigterm) {
        /* WAIT: Sleep until FDW wakes us up */
        WaitLatch(&MyProc->procLatch,
                  WL_LATCH_SET | WL_TIMEOUT,
                  1000,  /* 1 second timeout */
                  PG_WAIT_EXTENSION);

        ResetLatch(&MyProc->procLatch);

        /* PROCESS: Dequeue and execute all pending requests */
        GrpcRequest requests[MAX_PENDING_REQUESTS];
        uint32 count;

        if (dequeue_grpc_requests(requests, &count)) {
            for (uint32 i = 0; i < count; i++) {
                GrpcRequest *req = &requests[i];

                /* EXECUTE: Call gRPC Check */
                bool allowed = grpc_check_permission(
                    req->object_type, req->object_id,
                    req->relation,
                    req->subject_type, req->subject_id);

                /* RESPOND: Write result back and signal backend */
                set_grpc_result(req->request_id, allowed, 0);
                /* set_grpc_result() calls SetLatch(backend_latch) */
            }
        }
    }
}
```

#### `dequeue_grpc_requests()`
- **Called by**: BGW main loop
- **Action**: Retrieve all pending requests from queue
- **Effect**: Clears the queue after copying
- **Synchronization**: Acquires LWLock (exclusive) briefly

```c
bool dequeue_grpc_requests(
    GrpcRequest *requests,
    uint32 *count);
```

#### `set_grpc_result()`
- **Called by**: BGW (after gRPC call completes)
- **Action**: Write result to shared memory, signal backend
- **Synchronization**: Acquires LWLock (exclusive)

```c
void set_grpc_result(
    uint32 request_id,
    bool allowed,
    uint32 error_code);
```

#### `set_bgw_latch()`
- **Called by**: BGW during initialization
- **Action**: Register BGW's Latch in shared memory
- **Purpose**: Allows FDWs to wake up BGW

```c
void set_bgw_latch(volatile uint32 *latch);
```

## Synchronization Details

### LWLock Usage

All shared memory accesses are protected by a single `LWLock`:

| Operation | Lock Mode | Held Time | Comment |
|-----------|-----------|-----------|---------|
| Enqueue request | LW_EXCLUSIVE | Brief | Add to queue, increment counter |
| Dequeue requests | LW_EXCLUSIVE | Brief | Copy queue, clear counter |
| Write result | LW_EXCLUSIVE | Brief | Update status and result fields |
| Check result | LW_SHARED | Brief | Poll status field |

### PostgreSQL Latches

PostgreSQL's `Latch` mechanism provides efficient event signaling:

1. **WaitLatch()**: Block until signaled or timeout
   - No busy-waiting (efficient)
   - Can be interrupted by signals
   - Returns immediately if Latch already set

2. **SetLatch()**: Signal a Latch
   - Wakes up process blocked in WaitLatch()
   - Can be called from other processes

3. **ResetLatch()**: Clear Latch before waiting
   - Prevents race conditions

### Timeout Handling

**FDW Timeout**: 5 seconds max per request
- If BGW doesn't respond in 5 seconds, FDW returns error
- Backend falls back to "deny" mode (safe default)

**BGW Timeout**: 1 second between polls
- Checks for new requests every 1 second
- Can process multiple requests per poll

## Performance Characteristics

### Throughput

- **Cache hits**: < 1ms (no gRPC, no Latch)
- **Cache miss, single request**: 10-100ms (gRPC delay)
  - FDW enqueue: < 1ms
  - FDW wait (blocks): 10-100ms (gRPC execution)
  - BGW dequeue + gRPC: 10-100ms
- **Batch processing**: N requests in ~(10+100)ms (amortized)
  - Multiple FDWs queue requests
  - BGW dequeues and batches gRPC calls
  - All backends awoken simultaneously

### Concurrency

- **Multiple FDWs**: Supported (queue is shared)
- **Lock contention**: Minimal
  - LWLock held only for queue operations (< 1ms)
  - WaitLatch() and gRPC calls don't hold lock
- **Latches**: No contention (each process has own Latch)

## Error Handling

### Request Queue Full

```c
if (state->pending_request_count >= MAX_PENDING_REQUESTS) {
    ereport(ERROR, "gRPC request queue is full");
}
```

Default: `MAX_PENDING_REQUESTS = 256`

### BGW Latch Not Available

```c
if (state->bgw_latch == NULL) {
    elog(WARNING, "BGW latch not available");
    return;  /* Fall back to caching only */
}
```

### Timeout While Waiting

```c
if (rc & WL_TIMEOUT) {
    elog(WARNING, "Timeout waiting for gRPC result");
    *allowed = false;  /* Safe default: deny */
    return false;
}
```

### Lost Signal Race Condition Prevention

PostgreSQL's `WaitLatch()` handles this correctly:

```c
/* Check condition BEFORE waiting */
if (result_ready) {
    return result;
}

/* Acquire lock only if not ready */
rc = WaitLatch(&MyProc->procLatch,
               WL_LATCH_SET | WL_TIMEOUT,
               timeout,
               PG_WAIT_EXTENSION);

/* Check again after waking up */
if (result_ready) {
    return result;
}
```

If `SetLatch()` is called between the check and `WaitLatch()`, the Latch remains set and `WaitLatch()` returns immediately.

## Cache Integration

### Generation-based Invalidation

When BGW processes a change from OpenFGA, it increments generation counters:

```c
/* Change: document#123 permission changed */
increment_generation("document_type");     /* All documents stale */
increment_generation("document#123");       /* This document stale */
```

FDW checks cache staleness:

```c
if (is_cache_stale(&cache_entry)) {
    /* Generation mismatch detected */
    /* Enqueue new gRPC request */
}
```

### Result Caching (Future)

When BGW completes a request, it can optionally cache the result:

```c
/* In set_grpc_result() - optional optimization */
cache_insert(&cache_entry_from_grpc_result);
```

This avoids repeated requests for same permission tuple.

## Configuration

### Queue Size

```c
#define MAX_PENDING_REQUESTS 256
```

Adjustable based on system load and memory availability.

### Request Timeout

```c
#define GRPC_RESULT_TIMEOUT 5000L  /* milliseconds */
```

Prevents FDW from blocking indefinitely.

### BGW Poll Timeout

```c
WaitLatch(..., 1000, ...)  /* 1 second */
```

Allows BGW to process requests promptly.

## Example Session

### Session 1: User Query (FDW)

```sql
-- User issues query checking if alice can read document 123
SELECT 1 FROM acl_permission
 WHERE object_type = 'document'
   AND object_id = '123'
   AND subject_type = 'user'
   AND subject_id = 'alice'
   AND relation = 'read'
LIMIT 1;
```

### FDW Processing

```
[FDW] Check cache for (document, 123, user, alice, read)
[FDW] Cache miss!
[FDW] Enqueue request #1 with backend_latch pointer
[FDW] Call notify_bgw_of_pending_work()
[FDW] Call wait_for_grpc_result(1, ...)
[FDW] WaitLatch() blocks...
```

### BGW Processing

```
[BGW] WaitLatch() wakes up (SetLatch from FDW)
[BGW] Call dequeue_grpc_requests()
[BGW] Get request #1: (document, 123, user, alice, read)
[BGW] Call gRPC Check → OpenFGA API
[BGW] OpenFGA returns: allowed = true
[BGW] Call set_grpc_result(1, true, 0)
[BGW] set_grpc_result() calls SetLatch(backend_latch)
[BGW] Loop back to WaitLatch()
```

### FDW Resumes

```
[FDW] WaitLatch() returns
[FDW] Read result from shared memory: allowed = true
[FDW] Return 1 row to SQL
```

### Query Result

```
 ?column?
──────────
        1
(1 row)
```

## Testing

### Unit Tests

```c
/* Test enqueue/dequeue */
test_request_queue_basic();

/* Test multiple concurrent FDWs */
test_request_queue_concurrent();

/* Test queue full error */
test_request_queue_full();

/* Test timeout handling */
test_wait_timeout();

/* Test result reading */
test_result_reading();
```

### Integration Tests

```c
/* Full request-response cycle */
test_request_response_cycle();

/* Multiple requests batching */
test_batch_processing();

/* BGW crash recovery */
test_bgw_restart();

/* Concurrent FDWs and BGW */
test_concurrent_fdw_bgw();
```

## Future Enhancements

1. **Priority Queue**: Process high-priority requests first
2. **Request Timeout**: Individual request timeouts
3. **Batch gRPC Calls**: Combine multiple requests into one gRPC call
4. **Metrics**: Request count, latency distribution, cache hit rate
5. **Adaptive Caching**: Adjust TTL based on change frequency
6. **Request Deduplication**: Merge identical concurrent requests

## References

- PostgreSQL Latches: `src/backend/storage/latch/latch.c`
- PostgreSQL LWLocks: `src/backend/storage/lmgr/lwlock.c`
- gRPC C++ Client: `src/grpc/client.cpp`
- Shared Memory Structures: `include/shared_memory.h`
