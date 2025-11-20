# OpenFGA gRPC Client - 빠른 시작

## 🚀 빌드

### 1. Extension 빌드 (기본)

```bash
cd /workspace
mkdir build && cd build
cmake ..
cmake --build .
```

### 2. 테스트 프로그램 포함 빌드

```bash
cmake .. -DBUILD_GRPC_TEST=ON
cmake --build .
```

## 📦 생성된 파일

```
build/
├── postfga.so              # PostgreSQL extension
└── test_grpc_client        # gRPC 클라이언트 테스트 프로그램 (옵션)
```

## 🧪 테스트 실행

### OpenFGA 서버 시작

```bash
docker run -d --name openfga \
  -p 8080:8080 \
  -p 8081:8081 \
  openfga/openfga run
```

### Store 생성

```bash
# HTTP API로 store 생성
curl -X POST http://localhost:8080/stores \
  -H "Content-Type: application/json" \
  -d '{"name": "test-store"}'

# 응답에서 store_id 확인 (예: 01HXXX...)
```

### 테스트 실행

```bash
# gRPC 클라이언트 테스트
./build/test_grpc_client dns:///localhost:8081 01HXXX
```

**예상 출력:**

```
=== OpenFGA gRPC Client Test ===

Endpoint: dns:///localhost:8081
Store ID: 01HXXX

[1] Initializing gRPC client...
    ✓ Client initialized

[2] Checking client health...
    ✓ Client is healthy

[3] Testing synchronous Check...
    Checking: user:alice can read document:test-doc-123
    ✓ Check completed
    Result: DENIED

[4] Testing asynchronous Check...
    Checking: user:bob can write document:test-doc-456
    ✓ Async check queued

[ASYNC] Callback for request: async-test-001
[ASYNC] Result: DENIED
    ✓ Async check completed

[5] Testing multiple rapid checks...
    Completed 16/16 checks

[6] Shutting down client...
    ✓ Client shutdown complete

=== Test Complete ===
```

## 💡 코드 예제

### 최소 예제

```c
#include "grpc_client.h"

int main() {
    // 1. 클라이언트 초기화
    GrpcClient *client = grpc_client_init("dns:///localhost:8081");

    // 2. 권한 체크
    CheckRequest req = {
        .store_id = "01HXXX",
        .object_type = "document",
        .object_id = "123",
        .relation = "read",
        .subject_type = "user",
        .subject_id = "alice"
    };

    CheckResponse resp;

    if (grpc_client_check_sync(client, &req, &resp)) {
        printf("Allowed: %s\n", resp.allowed ? "YES" : "NO");
    }

    // 3. 종료
    grpc_client_shutdown(client);

    return 0;
}
```

### 비동기 예제

```c
void my_callback(const CheckResponse *resp, void *user_data) {
    printf("Permission check result: %s\n",
           resp->allowed ? "ALLOWED" : "DENIED");
}

int main() {
    GrpcClient *client = grpc_client_init("dns:///localhost:8081");

    CheckRequest req = { /* ... */ };

    // 비동기 호출 (콜백은 백그라운드 스레드에서 자동 실행)
    grpc_client_check_async(client, &req, my_callback, NULL);

    // 다른 작업 수행...

    grpc_client_shutdown(client);
}
```

## 🔧 CMake 통합

### 프로젝트에서 사용

```cmake
# OpenFGA gRPC 클라이언트 포함
include(cmake/openfga.cmake)

# 소스 파일에 추가
add_executable(my_app
    main.c
    ${CMAKE_SOURCE_DIR}/src/grpc_client.cpp
)

# Include 디렉터리
target_include_directories(my_app PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OPENFGA_PROTO_INCLUDE_DIR}
    ${OPENFGA_GRPC_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
)

# 컴파일 정의
target_compile_definitions(my_app PRIVATE
    ASIO_STANDALONE
)

# 라이브러리 링크
target_link_libraries(my_app PRIVATE
    openfga_api
    gRPC::grpc++
    protobuf::libprotobuf
    pthread
)
```

## 🐛 트러블슈팅

### "Failed to initialize gRPC client"

**원인**: OpenFGA 서버가 실행 중이 아님

**해결**:
```bash
docker ps | grep openfga
# 없으면 시작
docker run -d -p 8080:8080 -p 8081:8081 openfga/openfga run
```

### "Check failed: UNAVAILABLE"

**원인**: gRPC 엔드포인트에 연결할 수 없음

**해결**:
```bash
# 엔드포인트 확인
grpc_cli ls localhost:8081

# 방화벽/포트 확인
netstat -tlnp | grep 8081
```

### "Error 3: INVALID_ARGUMENT"

**원인**: store_id가 잘못됨

**해결**:
```bash
# 올바른 store_id 확인
curl http://localhost:8080/stores
```

### 빌드 오류: "openfga/v1/openfga.pb.h not found"

**원인**: FetchContent가 proto 파일을 다운로드하지 못함

**해결**:
```bash
# 캐시 삭제 후 재빌드
rm -rf build/_deps
cmake --build build --clean-first
```

## 📚 다음 단계

1. **상세 가이드**: [GRPC_CLIENT_GUIDE.md](GRPC_CLIENT_GUIDE.md)
2. **API 레퍼런스**: [include/grpc_client.h](include/grpc_client.h)
3. **OpenFGA 문서**: https://openfga.dev/

## 🎯 주요 특징

✅ **FetchContent 통합**: CMake가 자동으로 OpenFGA proto 다운로드
✅ **Standalone ASIO**: 별도 Boost 설치 불필요
✅ **비동기 I/O**: 백그라운드 스레드에서 자동 처리
✅ **C 인터페이스**: PostgreSQL extension에서 직접 사용 가능
✅ **제로 복사**: 효율적인 메모리 관리

## 📝 의존성

### 자동으로 다운로드됨 (FetchContent)
- OpenFGA Protobuf definitions
- OpenFGA gRPC service definitions
- Standalone ASIO

### 시스템에 필요
- gRPC C++ (libgrpc++)
- Protocol Buffers (libprotobuf)
- CMake 3.20+
- C++20 compiler

### 설치 (Ubuntu/Debian)

```bash
apt-get install -y \
    build-essential \
    cmake \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc
```
