# PostFGA Test Framework Guide

## 테스트 구조

```
tests/
├── sql/                    # PostgreSQL regression tests
│   ├── test_queue.sql
│   ├── test_cache.sql
│   └── test_guc.sql
├── expected/               # Expected outputs
│   ├── test_queue.out
│   ├── test_cache.out
│   └── test_guc.out
├── unit/                   # C/C++ unit tests (optional)
│   ├── test_client.cpp
│   └── test_utils.cpp
└── integration/            # Integration tests
    └── test_full_stack.sh
```

## 1. SQL 기반 테스트 (현재 방식)

### 작성 방법:

```sql
-- sql/test_queue.sql
-- Test: Enqueue requests
SELECT fga_test_enqueue(5) AS enqueued_count;
-- Expected: 5

-- Test: Queue stats
SELECT fga_test_queue_stats();
-- Expected: Queue: 5/256 (2.0% full)

-- Test: Clear queue
SELECT fga_test_clear_queue();
-- Expected: 5
```

### 실행:

```bash
# 수동 실행
psql -U postgres -f sql/test_queue.sql > results/test_queue.out

# 비교
diff expected/test_queue.out results/test_queue.out
```

---

## 2. C 함수 테스트 (PostgreSQL 내부)

각 모듈에 대한 테스트 함수 작성:

```c
// tests/test_cache.c
PG_FUNCTION_INFO_V1(fga_test_cache_insert);
Datum fga_test_cache_insert(PG_FUNCTION_ARGS) {
    // Test cache insertion
    // Return true/false
}

PG_FUNCTION_INFO_V1(fga_test_cache_lookup);
Datum fga_test_cache_lookup(PG_FUNCTION_ARGS) {
    // Test cache lookup
}
```

### SQL에서 호출:

```sql
-- sql/test_cache.sql
CREATE FUNCTION fga_test_cache_insert() RETURNS bool
AS 'MODULE_PATHNAME' LANGUAGE C;

SELECT fga_test_cache_insert() AS "Cache Insert Test";
```

---

## 3. GTest 추가 (선택사항)

### CMakeLists.txt:

```cmake
option(BUILD_GTEST "Build Google Test unit tests" OFF)

if(BUILD_GTEST)
    find_package(GTest REQUIRED)
    enable_testing()

    add_executable(unit_tests
        tests/unit/test_client.cpp
    )

    target_link_libraries(unit_tests
        GTest::GTest
        GTest::Main
        openfga
    )

    gtest_discover_tests(unit_tests)
endif()
```

### 테스트 작성:

```cpp
// tests/unit/test_client.cpp
#include <gtest/gtest.h>
#include "client.h"

class ClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client = fga_client_init("localhost:8080");
    }

    void TearDown() override {
        fga_client_fini(client);
    }

    GrpcClient *client;
};

TEST_F(ClientTest, ClientInitialization) {
    EXPECT_NE(client, nullptr);
    EXPECT_TRUE(fga_client_is_healthy(client));
}

TEST_F(ClientTest, SyncCheck) {
    GrpcCheckRequest req = {
        .store_id = "test-store",
        .object_type = "document",
        .object_id = "1",
        .relation = "reader",
        .subject_type = "user",
        .subject_id = "alice"
    };

    CheckResponse resp;
    bool success = fga_client_check_sync(client, &req, &resp);

    EXPECT_TRUE(success);
}
```

---

## 4. 통합 테스트 스크립트

```bash
#!/bin/bash
# tests/integration/test_full_stack.sh

set -e

echo "Starting integration tests..."

# 1. Start OpenFGA
docker run -d --name openfga-test -p 8080:8080 openfga/openfga run
sleep 2

# 2. Start PostgreSQL
pg_ctl -D /tmp/test_pgdata start

# 3. Run SQL tests
psql -U postgres -f sql/test_queue.sql

# 4. Check BGW logs
grep "PostFGA BGW" /tmp/test_pgdata/logfile

# 5. Cleanup
pg_ctl -D /tmp/test_pgdata stop
docker stop openfga-test && docker rm openfga-test

echo "All tests passed!"
```

---

## 5. CI/CD 통합 (GitHub Actions 예시)

```yaml
# .github/workflows/test.yml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest

    services:
      postgres:
        image: postgres:18
        env:
          POSTGRES_PASSWORD: postgres
        options: >-
          --health-cmd pg_isready
          --health-interval 10s

      openfga:
        image: openfga/openfga
        ports:
          - 8080:8080

    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y postgresql-server-dev-18

      - name: Build
        run: |
          mkdir build && cd build
          cmake ..
          make
          sudo make install

      - name: Run tests
        run: |
          psql -U postgres -h localhost -f sql/test_queue.sql
          psql -U postgres -h localhost -f sql/test_cache.sql
```

---

## 권장사항

현재 프로젝트에는 **SQL 기반 테스트 (옵션 1)** 를 권장합니다:

1. ✅ 이미 `test_queue.c` + `test_queue.sql` 구현됨
2. ✅ PostgreSQL extension에 가장 적합
3. ✅ 실제 환경에서 테스트
4. ✅ 설정이 간단

추가로 필요하다면:
- `client.cpp` → GTest (독립 실행형)
- 통합 테스트 → Bash 스크립트

## 빠른 시작

```bash
# 1. 테스트 함수 빌드
cd /workspace/build
cmake --build . && cmake --install .

# 2. PostgreSQL 재시작
pg_ctl -D /tmp/pgdata -l /tmp/pgdata/logfile restart

# 3. 테스트 실행
psql -U postgres -f /workspace/sql/test_queue.sql

# 4. 결과 확인
echo $?  # 0이면 성공
```
