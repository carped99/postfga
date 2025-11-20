# OpenFGA gRPC Client - 사용 가이드

## 개요

Standalone ASIO를 사용한 비동기 OpenFGA gRPC 클라이언트입니다.

### 주요 기능

- ✅ **비동기 I/O**: Standalone ASIO 기반 이벤트 루프
- ✅ **동기/비동기 API**: 필요에 따라 선택 가능
- ✅ **FetchContent 통합**: CMake FetchContent로 OpenFGA proto 자동 다운로드
- ✅ **스레드 안전**: 멀티스레드 환경에서 안전하게 사용
- ✅ **C/C++ 인터페이스**: C에서 호출 가능한 extern "C" API

## 아키텍처

```
┌─────────────────────────────────────────┐
│  PostgreSQL Backend / BGW               │
│  ┌─────────────────────────────────┐    │
│  │   C API (grpc_client.h)         │    │
│  └──────────────┬──────────────────┘    │
│                 │                        │
│  ┌──────────────▼──────────────────┐    │
│  │   C++ Implementation            │    │
│  │   (grpc_client.cpp)             │    │
│  │                                 │    │
│  │   ┌─────────────────────────┐   │    │
│  │   │  Standalone ASIO        │   │    │
│  │   │  Event Loop             │   │    │
│  │   └─────────────────────────┘   │    │
│  │                                 │    │
│  │   ┌─────────────────────────┐   │    │
│  │   │  gRPC Client            │   │    │
│  │   │  Completion Queue       │   │    │
│  │   └─────────────────────────┘   │    │
│  └─────────────────────────────────┘    │
└─────────────────┬───────────────────────┘
                  │
                  │ gRPC
                  ▼
         ┌────────────────┐
         │  OpenFGA       │
         │  Server        │
         └────────────────┘
```

## CMake 설정

### FetchContent로 의존성 자동 다운로드

`cmake/openfga.cmake`:

```cmake
include(FetchContent)

# OpenFGA Protobuf
FetchContent_Declare(openfga_proto
    URL https://buf.build/gen/cmake/openfga/api/protocolbuffers/cpp/v33.1-47cab76c95bf.1
)

# OpenFGA gRPC
FetchContent_Declare(openfga_grpc
    URL https://buf.build/gen/cmake/openfga/api/grpc/cpp/v1.76.0-47cab76c95bf.1
)

# Standalone ASIO
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
)

FetchContent_MakeAvailable(openfga_proto openfga_grpc asio)
```

### 빌드

```bash
cd /workspace
mkdir build && cd build
cmake ..
cmake --build .
```

## API 사용법

### 1. 클라이언트 초기화

```c
#include "grpc_client.h"

// 클라이언트 생성
GrpcClient *client = grpc_client_init("dns:///localhost:8081");
if (!client) {
    fprintf(stderr, "Failed to initialize gRPC client\n");
    return -1;
}

// 헬스 체크
if (!grpc_client_is_healthy(client)) {
    fprintf(stderr, "gRPC client is not healthy\n");
}
```

### 2. 동기 Check API 호출

```c
// 요청 준비
CheckRequest req = {
    .store_id = "01HXXX",
    .authorization_model_id = NULL,  // Optional
    .object_type = "document",
    .object_id = "123",
    .relation = "read",
    .subject_type = "user",
    .subject_id = "alice"
};

// 응답
CheckResponse resp;

// 동기 호출
if (grpc_client_check_sync(client, &req, &resp)) {
    if (resp.allowed) {
        printf("Access granted\n");
    } else {
        printf("Access denied\n");
    }
} else {
    fprintf(stderr, "Check failed: %s\n", resp.error_message);
}
```

### 3. 비동기 Check API 호출

```c
// 콜백 함수
void check_callback(const CheckResponse *resp, void *user_data)
{
    const char *request_id = (const char *)user_data;

    printf("[%s] Check completed\n", request_id);

    if (resp->error_code == 0) {
        printf("[%s] Allowed: %s\n", request_id,
               resp->allowed ? "YES" : "NO");
    } else {
        printf("[%s] Error: %s\n", request_id, resp->error_message);
    }
}

// 비동기 호출
CheckRequest req = {
    .store_id = "01HXXX",
    .object_type = "document",
    .object_id = "456",
    .relation = "write",
    .subject_type = "user",
    .subject_id = "bob"
};

const char *request_id = "req-001";

if (grpc_client_check_async(client, &req, check_callback, (void *)request_id)) {
    printf("Async check queued successfully\n");
} else {
    fprintf(stderr, "Failed to queue async check\n");
}

// 이벤트 루프는 백그라운드 스레드에서 자동으로 처리됨
```

### 4. 클라이언트 종료

```c
// 모든 요청 완료 대기 후 종료
grpc_client_shutdown(client);
```

## Background Worker 통합 예제

```c
// BGW에서 gRPC 클라이언트 사용

static GrpcClient *bgw_grpc_client = NULL;

void bgw_main(Datum main_arg)
{
    // 초기화
    bgw_grpc_client = grpc_client_init("dns:///openfga:8081");

    // 이벤트 루프
    while (!got_sigterm) {
        // OpenFGA 변경 이벤트 처리
        process_openfga_changes(bgw_grpc_client);

        // 대기
        WaitLatch(MyLatch,
                  WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
                  1000L,
                  PG_WAIT_EXTENSION);

        ResetLatch(MyLatch);

        CHECK_FOR_INTERRUPTS();
    }

    // 종료
    grpc_client_shutdown(bgw_grpc_client);
}

void process_openfga_changes(GrpcClient *client)
{
    // OpenFGA ReadChanges 호출 (구현 필요)
    // 변경된 튜플에 대해 캐시 invalidation 수행
}
```

## FDW 통합 예제

```c
// FDW 스캔에서 캐시 미스 시 gRPC 호출

static void
check_permission_on_cache_miss(const char *object_type,
                               const char *object_id,
                               const char *subject_type,
                               const char *subject_id,
                               const char *relation,
                               bool *allowed)
{
    CheckRequest req = {
        .store_id = get_config()->store_id,
        .object_type = object_type,
        .object_id = object_id,
        .relation = relation,
        .subject_type = subject_type,
        .subject_id = subject_id
    };

    CheckResponse resp;

    if (grpc_client_check_sync(global_grpc_client, &req, &resp)) {
        *allowed = resp.allowed;

        // 캐시에 저장
        cache_insert_result(object_type, object_id, subject_type,
                          subject_id, relation, resp.allowed);
    } else {
        elog(WARNING, "gRPC check failed: %s", resp.error_message);
        *allowed = false;
    }
}
```

## 성능 최적화

### 1. 연결 풀링

```c
// 여러 BGW 워커가 하나의 gRPC 채널 공유
// gRPC 채널은 내부적으로 연결 풀링 수행
```

### 2. 배치 처리 (향후 구현)

```c
// 여러 Check 요청을 하나의 gRPC 호출로 묶기
// OpenFGA BatchCheck API 사용
```

### 3. 캐싱 전략

```c
// 1. 먼저 캐시 조회
// 2. 캐시 미스 시에만 gRPC 호출
// 3. 결과를 캐시에 저장
// 4. Generation 기반 무효화
```

## 에러 처리

### 에러 코드

| 코드 | 의미 |
|------|------|
| 0 | 성공 |
| 1-16 | gRPC 표준 에러 코드 |
| 1000 | 내부 예외 |

### gRPC 에러 코드 참조

- `1`: CANCELLED
- `2`: UNKNOWN
- `3`: INVALID_ARGUMENT
- `4`: DEADLINE_EXCEEDED
- `5`: NOT_FOUND
- `6`: ALREADY_EXISTS
- `7`: PERMISSION_DENIED
- `14`: UNAVAILABLE

### 재시도 로직 예제

```c
bool check_with_retry(GrpcClient *client, const CheckRequest *req, CheckResponse *resp)
{
    int max_retries = 3;
    int retry_delay_ms = 100;

    for (int i = 0; i < max_retries; i++) {
        if (grpc_client_check_sync(client, req, resp)) {
            return true;
        }

        // UNAVAILABLE이면 재시도
        if (resp->error_code == 14) {
            usleep(retry_delay_ms * 1000);
            retry_delay_ms *= 2;  // Exponential backoff
            continue;
        }

        // 다른 에러는 재시도하지 않음
        break;
    }

    return false;
}
```

## 디버깅

### 로깅 활성화

```c
// gRPC 환경 변수 설정
setenv("GRPC_VERBOSITY", "DEBUG", 1);
setenv("GRPC_TRACE", "all", 1);
```

### 통계 확인

```c
// 클라이언트 내부 통계 (향후 구현)
printf("Total requests: %lu\n", client->total_requests);
printf("Total errors: %lu\n", client->total_errors);
```

## 제한사항

1. **현재 구현**:
   - Check API만 구현됨
   - ReadChanges, Write, Expand 등은 향후 추가 예정

2. **성능**:
   - 각 Check 호출은 별도의 gRPC 요청
   - 배치 처리는 향후 BatchCheck로 구현 예정

3. **스레드 모델**:
   - 완료 큐는 전용 스레드에서 처리
   - 콜백은 완료 큐 스레드에서 실행 (주의 필요)

## 트러블슈팅

### 연결 실패

```bash
# OpenFGA 서버 확인
curl http://localhost:8080/healthz

# gRPC 포트 확인
grpc_cli ls localhost:8081
```

### 빌드 오류

```bash
# FetchContent 캐시 삭제
rm -rf build/_deps

# 재빌드
cmake --build build --clean-first
```

### 런타임 오류

```bash
# 라이브러리 경로 확인
ldd build/postfga.so

# 누락된 경우 LD_LIBRARY_PATH 설정
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

## 참고 자료

- [OpenFGA API Documentation](https://openfga.dev/api)
- [gRPC C++ Documentation](https://grpc.io/docs/languages/cpp/)
- [Standalone ASIO](https://think-async.com/Asio/)
- [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)
