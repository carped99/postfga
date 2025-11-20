// postfga_shmem.hpp
#pragma once

extern "C" {
#include "postgres.h"
#include "storage/lwlock.h"
#include "storage/latch.h"
#include "storage/shmem.h"
#include "utils/timestamp.h"
}

#include <cstdint>

namespace postfga {

// 이름/사이즈 상수
constexpr int RELATION_NAME_MAX      = 64;
constexpr int MAX_PENDING_REQUESTS   = 256;

// 요청 상태
enum class RequestStatus : std::uint8_t {
    Pending   = 0,
    Completed = 1,
    Error     = 2,
};

// gRPC 요청 (shared memory에 올라가는 POD 구조체)
struct GrpcRequest {
    std::uint32_t  request_id;
    RequestStatus  status;

    pid_t          backend_pid;

    char           object_type[RELATION_NAME_MAX];
    char           object_id[RELATION_NAME_MAX];
    char           subject_type[RELATION_NAME_MAX];
    char           subject_id[RELATION_NAME_MAX];
    char           relation[RELATION_NAME_MAX];

    bool           allowed;
    std::uint32_t  error_code;

    TimestampTz    created_at;
};

// shared state (POD)
struct PostfgaShmemState {
    LWLock*   lock;       // named tranche에서 가져옴
    Latch*    bgw_latch;  // BGW에서 등록

    // 간단한 ring buffer 큐
    std::uint32_t queue_head;
    std::uint32_t queue_tail;
    std::uint32_t next_request_id;
    GrpcRequest   pending_requests[MAX_PENDING_REQUESTS];

    // TODO: 나중에 캐시/통계/세대(generation) 등 추가
};

// ---- shmem API ----

// shared state 포인터 가져오기 (nullptr 가능)
PostfgaShmemState* shared();

// shmem_request_hook 에서 호출할 함수
void shmem_request();

// shmem_startup_hook 에서 호출할 함수
void shmem_startup();

// PG_init 에서 호출할 초기화/후처리
void on_init();
void on_fini();

// ---- 큐 API (C++ 전용, FDW/BGW에서 사용) ----

bool enqueue_request(const GrpcRequest& req, std::uint32_t& out_request_id);
bool dequeue_batch(GrpcRequest* buf, std::uint32_t max_count, std::uint32_t& out_count);

// BGW latch 등록/알림
void set_bgw_latch(Latch* latch);
void notify_bgw();

} // namespace postfga
