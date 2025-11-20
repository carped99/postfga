# Ring Buffer Queue 구현 문서

## 개요

PostFGA의 `queue.c`는 shared memory 기반의 **thread-safe ring buffer queue**를 구현합니다. 이 큐는 FDW 백엔드와 Background Worker 간의 gRPC 요청을 효율적으로 전달하는 데 사용됩니다.

**중요**: 이 구현은 **lock-free가 아닙니다**. PostgreSQL의 multi-process 아키텍처에서 안전성을 보장하기 위해 **LWLock + Atomic operations**을 사용합니다. 자세한 내용은 [MULTIPROCESS_SAFETY_ANALYSIS.md](MULTIPROCESS_SAFETY_ANALYSIS.md)를 참조하세요.

---

## 아키텍처

### 구조

```
┌─────────────────────────────────────────────┐
│         PostfgaShmemState                   │
│  ┌────────────────────────────────────────┐ │
│  │   GrpcRequest request_queue[256]       │ │
│  │   ┌────┬────┬────┬─────┬────┬────┐    │ │
│  │   │ 0  │ 1  │ 2  │ ... │254 │255 │    │ │
│  │   └────┴────┴────┴─────┴────┴────┘    │ │
│  │        ↑                     ↑          │ │
│  │      head                  tail         │ │
│  └────────────────────────────────────────┘ │
│                                              │
│  atomic queue_head  (consumer index)         │
│  atomic queue_tail  (producer index)         │
│  atomic queue_size  (current item count)     │
└──────────────────────────────────────────────┘
```

### 특징

1. **고정 크기**: `MAX_PENDING_REQ` (256) 엔트리
2. **Circular Buffer**: head/tail이 배열 끝에 도달하면 처음으로 돌아감
3. **LWLock 기반 동기화**: Multi-process 환경에서 안전성 보장
4. **Atomic Operations**: 추가 안전성 및 메모리 순서 보장
5. **Single Producer Pattern** (다중 FDW backends → LWLock으로 직렬화)
6. **Single Consumer**: Background Worker (dequeue)

---

## 핵심 함수

### 1. `enqueue_grpc_request()`

**목적**: FDW에서 호출하여 gRPC 요청을 큐에 추가

**프로세스**:
```c
1. 입력 검증 (object_type, object_id, subject_type, subject_id, relation)
2. shared state 획득
3. LWLock 획득 (LW_EXCLUSIVE)
4. queue_is_full() 체크
   ├─ Full → 에러 반환 (stats_inc_queue_full)
   └─ Not Full → 계속
5. tail 위치 읽기 (atomic)
6. request_queue[tail]에 데이터 복사
   ├─ request_id 생성
   ├─ status = PENDING
   ├─ backend_pid, backend_id 설정
   ├─ 문자열 복사 (strncpy)
   └─ created_at timestamp 설정
7. tail 증가 (circular: (tail + 1) % MAX_PENDING_REQ)
8. queue_size 증가 (atomic)
9. stats_inc_queue_enqueue()
10. LWLock 해제
11. notify_bgw_of_pending_work() → BGW에 알림
12. request_id 반환
```

**Thread Safety**:
- LWLock으로 보호
- Atomic operations (tail, size)

**Return**:
- Success: `request_id` (> 0)
- Failure: `0`

---

### 2. `dequeue_grpc_requests()`

**목적**: BGW에서 호출하여 큐에서 요청을 가져옴

**프로세스**:
```c
1. 입력 검증 (requests buffer, count)
2. shared state 획득
3. LWLock 획득 (LW_EXCLUSIVE)
4. queue_is_empty() 체크
   ├─ Empty → false 반환
   └─ Not Empty → 계속
5. head 위치 읽기 (atomic)
6. Loop (i = 0; i < max_count && !empty; i++):
   ├─ request_queue[head]를 output buffer로 memcpy
   ├─ head 증가 (circular: (head + 1) % MAX_PENDING_REQ)
   ├─ queue_size 감소 (atomic)
   └─ stats_inc_queue_dequeue()
7. LWLock 해제
8. *count = dequeued 개수
9. return true (if dequeued > 0)
```

**배치 처리**:
- 한 번에 여러 개의 요청을 dequeue 가능
- 최대 `max_count` 개까지 가져옴

---

### 3. `wait_for_grpc_result()`

**목적**: FDW가 gRPC 결과를 대기 (blocking)

**프로세스**:
```c
1. request_id로 큐를 polling (5초 타임아웃)
2. 10ms 간격으로 sleep
3. 각 iteration:
   ├─ LWLock 획득 (LW_SHARED)
   ├─ Linear search로 request_id 찾기
   ├─ status 확인:
   │  ├─ COMPLETED → allowed, error_code 복사 후 true 반환
   │  ├─ ERROR → false, error_code 복사 후 false 반환
   │  └─ PENDING → 계속 대기
   └─ LWLock 해제
4. 타임아웃 시 false 반환 (stats_inc_queue_timeout)
```

**개선 사항** (TODO):
- Condition variable 또는 Latch 사용
- Result hash table 사용 (O(1) lookup)
- Configurable timeout

---

### 4. `set_grpc_result()`

**목적**: BGW가 gRPC 결과를 설정

**프로세스**:
```c
1. request_id 검증
2. shared state 획득
3. LWLock 획득 (LW_EXCLUSIVE)
4. Linear search로 request_id 찾기
5. status == PENDING인 항목 찾으면:
   ├─ allowed = result
   ├─ error_code = code
   └─ status = COMPLETED or ERROR
6. LWLock 해제
```

---

### 5. `notify_bgw_of_pending_work()`

**목적**: BGW에게 작업이 있음을 알림

**프로세스**:
```c
1. shared state 획득
2. bgw_latch 체크 (NULL이 아닌지)
3. SetLatch(bgw_latch) → BGW 깨움
4. stats_inc_bgw_wakeup()
```

**Latch 사용**:
- BGW는 WaitLatch()로 대기
- FDW가 SetLatch() 호출 → BGW 즉시 깨어남

---

### 6. `clear_completed_requests()`

**목적**: 완료/에러 상태의 오래된 요청 정리

**프로세스**:
```c
1. shared state 획득
2. LWLock 획득 (LW_EXCLUSIVE)
3. Loop (i = 0; i < MAX_PENDING_REQ; i++):
   ├─ status == COMPLETED or ERROR 확인
   ├─ created_at 체크 (60초 이상 경과)
   └─ memset(req, 0) → 초기화
4. LWLock 해제
5. cleared 개수 반환
```

**호출 시점**:
- BGW의 메인 루프에서 주기적으로 호출
- 큐 공간 재사용

---

### 7. `get_queue_stats()`

**목적**: 큐 통계 조회

**Return**:
- `size`: 현재 큐 크기 (atomic read)
- `capacity`: 최대 용량 (MAX_PENDING_REQ)

---

## Ring Buffer 동작 원리

### Empty vs Full 판별

```c
// Empty
queue_size == 0

// Full
queue_size >= MAX_PENDING_REQ
```

### Circular Index 계산

```c
// Enqueue
tail = (tail + 1) % MAX_PENDING_REQ

// Dequeue
head = (head + 1) % MAX_PENDING_REQ
```

### 예시

```
초기 상태:
head=0, tail=0, size=0
[ ][ ][ ][ ]  (empty)

Enqueue 2개:
head=0, tail=2, size=2
[A][B][ ][ ]
 ↑     ↑
head  tail

Dequeue 1개:
head=1, tail=2, size=1
[x][B][ ][ ]
    ↑  ↑
   head tail

Enqueue 3개:
head=1, tail=0, size=4 (circular!)
[D][B][C][E]
 ↑  ↑
tail head
```

---

## Multi-Process Safety (중요!)

### PostgreSQL은 Multi-Process 아키텍처

PostgreSQL은 **multi-thread가 아닌 multi-process**를 사용합니다:
- 각 클라이언트 연결 = 별도 프로세스
- 프로세스 간 메모리 공유 = Shared Memory만 가능
- **동기화 필수**: Race condition 방지를 위해 Lock 필요

### LWLock (Primary Synchronization)

```c
// 모든 queue 수정은 LWLock으로 보호됨
LWLockAcquire(state->lock, LW_EXCLUSIVE);
{
    // Critical section - 다른 프로세스 진입 불가
    tail = pg_atomic_read_u32(&state->queue_tail);
    state->request_queue[tail] = new_request;
    pg_atomic_write_u32(&state->queue_tail, (tail + 1) % MAX);
}
LWLockRelease(state->lock);
```

**LWLock의 역할**:
- Race condition 방지
- Memory barrier 제공 (CPU reordering 방지)
- Cache coherency 보장

### Atomic Operations (Secondary)

```c
// Read
uint32 size = pg_atomic_read_u32(&state->queue_size);
uint32 head = pg_atomic_read_u32(&state->queue_head);
uint32 tail = pg_atomic_read_u32(&state->queue_tail);

// Write
pg_atomic_write_u32(&state->queue_tail, new_tail);

// Atomic increment/decrement
pg_atomic_fetch_add_u32(&state->queue_size, 1);
pg_atomic_fetch_sub_u32(&state->queue_size, 1);
```

### LWLock 사용

- **Exclusive**: Enqueue, Dequeue, Set result
- **Shared**: Wait for result (read-only)

### 동시성 시나리오

| 상황 | 처리 |
|------|------|
| 여러 FDW가 동시 enqueue | LWLock으로 직렬화 |
| BGW가 dequeue 중 FDW enqueue | LWLock으로 직렬화 |
| FDW가 wait 중 BGW가 result 설정 | Shared/Exclusive lock으로 안전 |

---

## 성능 특성

### 시간 복잡도

- **Enqueue**: O(1) (tail 위치에 직접 삽입)
- **Dequeue**: O(n) (최대 n개 배치 처리)
- **Wait for result**: O(k × m)
  - k = iterations (최대 500)
  - m = MAX_PENDING_REQ (256)
  - **TODO**: Hash table 사용 시 O(k)로 개선 가능
- **Set result**: O(m) (linear search)
  - **TODO**: Hash table 사용 시 O(1)로 개선

### 공간 복잡도

```c
sizeof(GrpcRequest) = ~320 bytes (approximate)
Total = 320 × 256 = ~80 KB
```

---

## 사용 예시

### FDW에서 요청 전송

```c
// 1. 요청 enqueue
uint32 req_id = enqueue_grpc_request(
    "document", "123",
    "user", "alice",
    "read"
);

if (req_id == 0) {
    // Queue full or error
    return deny;
}

// 2. 결과 대기
bool allowed;
uint32 error_code;
if (wait_for_grpc_result(req_id, &allowed, &error_code)) {
    return allowed ? allow : deny;
} else {
    // Timeout or error
    return deny;
}
```

### BGW에서 요청 처리

```c
// Main loop
while (!shutdown_requested) {
    // 1. 요청 dequeue
    GrpcRequest requests[32];
    uint32 count = 32;

    if (dequeue_grpc_requests(requests, &count)) {
        // 2. 각 요청 처리
        for (uint32 i = 0; i < count; i++) {
            bool allowed = call_openfga_check(&requests[i]);

            // 3. 결과 설정
            set_grpc_result(requests[i].request_id, allowed, 0);
        }
    }

    // 4. 주기적으로 오래된 요청 정리
    clear_completed_requests();

    // 5. 대기
    WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT, 1000, ...);
}
```

---

## 제한사항 및 개선 사항

### 현재 제한사항

1. **Linear Search**: `wait_for_grpc_result()`와 `set_grpc_result()`가 O(n)
2. **Polling**: Wait가 busy-waiting (10ms sleep)
3. **Fixed Size**: 큐 크기 고정 (256)
4. **Single Consumer**: 여러 BGW 지원 안 함

### 개선 계획

1. **Result Hash Table**
   ```c
   HTAB *result_map;  // request_id -> result
   ```
   - O(1) lookup/insert
   - Completed 요청 별도 관리

2. **Latch 기반 대기**
   ```c
   // FDW
   WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT, timeout, ...);

   // BGW (result 설정 후)
   SetLatch(backend_latch);
   ```

3. **동적 크기 조정**
   - GUC로 큐 크기 설정 가능
   - `openfga.max_queue_size`

4. **Multiple BGW 지원**
   - Work-stealing queue
   - Per-BGW queue partition

---

## 디버깅

### 로그 활성화

```sql
SET client_min_messages = DEBUG2;
```

### 주요 로그 메시지

```
DEBUG2: PostFGA: Enqueued request 42 (queue size: 5/256)
DEBUG2: PostFGA: Dequeued 3 requests (queue size: 2/256)
DEBUG2: PostFGA: Notified BGW of pending work
DEBUG2: PostFGA: Result received for request 42: allowed=1
```

### 큐 상태 확인

```c
uint32 size, capacity;
if (get_queue_stats(&size, &capacity)) {
    elog(LOG, "Queue: %u/%u", size, capacity);
}
```

---

## 참고 자료

- **PostgreSQL Atomic Operations**: `src/include/port/atomics.h`
- **LWLock**: `src/backend/storage/lmgr/lwlock.c`
- **Latch**: `src/backend/storage/ipc/latch.c`
- **Ring Buffer**: Classic SPSC pattern
