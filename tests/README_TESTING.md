# PostFGA Testing Guide

이 문서는 PostFGA 확장의 테스트 작성 및 실행 방법을 설명합니다.

## 테스트 종류

PostFGA는 세 가지 유형의 테스트를 제공합니다:

### 1. SQL Regression Tests (표준 PostgreSQL 방식)
- **파일**: `tests/sql/*.sql`, `tests/expected/*.out`
- **용도**: SQL을 통한 기능 테스트
- **장점**: PostgreSQL 표준, 설정이 간단
- **실행**: `make installcheck`

### 2. C Unit Tests
- **파일**: `tests/test_*.c`
- **용도**: C 함수의 단위 테스트
- **장점**: 세밀한 테스트, 빠른 실행
- **실행**: `make test-unit`

### 3. pgTAP Tests
- **파일**: `tests/sql/*_pgtap_test.sql`
- **용도**: TAP 프로토콜 기반 고급 테스트
- **장점**: 풍부한 assertion, CI/CD 통합 용이
- **실행**: `make test-pgtap` 또는 `pg_prove`

---

## 사전 요구사항

### 기본 요구사항
```bash
# PostgreSQL 개발 패키지
sudo apt-get install postgresql-server-dev-18

# PostgreSQL 서버 실행 중
pg_ctl status
```

### pgTAP 테스트 (선택사항)
```bash
# pgTAP 설치
git clone https://github.com/theory/pgtap.git
cd pgtap
make
sudo make install

# pg_prove 설치 (Perl TAP harness)
sudo cpan TAP::Parser::SourceHandler::pgTAP
```

---

## 테스트 실행 방법

### 빠른 시작
```bash
cd tests/
make test
```

### 개별 테스트 실행

#### 1. SQL Regression Test
```bash
# 방법 1: Makefile 사용
cd tests/
make test-sql

# 방법 2: 직접 실행
psql -d postgres -f sql/guc_test.sql

# 방법 3: PostgreSQL regression test
make installcheck REGRESS=guc_test
```

**출력 예시:**
```sql
SHOW openfga.endpoint;
      openfga.endpoint
-----------------------------
 dns:///localhost:8081
(1 row)

SET openfga.cache_ttl_ms = 30000;
SET
SHOW openfga.cache_ttl_ms;
 openfga.cache_ttl_ms
----------------------
 30000
(1 row)
```

#### 2. C Unit Test
```bash
cd tests/
make test-unit
```

**출력 예시:**
```
[TEST 1] Config Initialization
  ✓ PASS: Config pointer should not be NULL
  ✓ PASS: Default endpoint should be set
  ✓ PASS: Default relations should be set

[TEST 2] Config Value Ranges
  ✓ PASS: Cache TTL should be >= 1000ms
  ✓ PASS: Cache TTL should be <= 3600000ms

Test Summary
=================================================
Total Tests:  5
Passed:       12
Failed:       0
=================================================
✓ All tests passed!
```

#### 3. pgTAP Test
```bash
# 방법 1: pg_prove 사용 (권장)
cd tests/
pg_prove -d postgres sql/guc_pgtap_test.sql

# 방법 2: 직접 psql 실행
psql -d postgres -f sql/guc_pgtap_test.sql
```

**출력 예시:**
```
sql/guc_pgtap_test.sql .. ok
All tests successful.
Files=1, Tests=30, 2 wallclock secs
Result: PASS
```

---

## 테스트 작성 가이드

### SQL Regression Test 작성

**파일 구조:**
```
tests/
  sql/
    my_feature_test.sql      # 테스트 SQL
  expected/
    my_feature_test.out      # 예상 출력
```

**예제:**
```sql
-- tests/sql/my_feature_test.sql
-- Test my new feature

-- Setup
CREATE EXTENSION postfga_fdw;

-- Test case 1
SELECT 1 AS test_value;

-- Test case 2
SET openfga.endpoint = 'dns:///test:8081';
SHOW openfga.endpoint;

-- Cleanup
DROP EXTENSION postfga_fdw;
```

**예상 출력 생성:**
```bash
# 첫 실행 시 출력을 캡처하여 expected 파일 생성
psql -d postgres -f sql/my_feature_test.sql > expected/my_feature_test.out
```

### C Unit Test 작성

**템플릿:**
```c
#include <postgres.h>
#include <assert.h>

static void test_my_function(void)
{
    TEST_START("My Function Test");

    // Arrange
    int input = 42;

    // Act
    int result = my_function(input);

    // Assert
    TEST_ASSERT_INT_EQ(result, 42, "Function should return input");
}

// 테스트 실행에 추가
static void run_all_tests(void)
{
    test_my_function();
    // ... 다른 테스트들
}
```

### pgTAP Test 작성

**템플릿:**
```sql
BEGIN;
SELECT plan(3);  -- 테스트 개수

-- Test 1: Function exists
SELECT has_function('my_function');

-- Test 2: Correct return type
SELECT function_returns('my_function', 'integer');

-- Test 3: Function works correctly
SELECT is(
    my_function(42),
    42,
    'my_function should return its input'
);

SELECT * FROM finish();
ROLLBACK;
```

---

## CI/CD 통합

### GitHub Actions 예제

```yaml
name: PostgreSQL Tests

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
          --health-timeout 5s
          --health-retries 5

    steps:
    - uses: actions/checkout@v2

    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y postgresql-server-dev-18

    - name: Build extension
      run: make USE_PGXS=1

    - name: Install extension
      run: sudo make install USE_PGXS=1

    - name: Run tests
      run: |
        cd tests
        make test
      env:
        PGHOST: localhost
        PGPORT: 5432
        PGUSER: postgres
        PGPASSWORD: postgres
        PGDATABASE: postgres
```

---

## 테스트 디버깅

### SQL 테스트 디버깅
```bash
# VERBOSE 모드로 실행
psql -d postgres -f sql/guc_test.sql -e

# 에러 상세 정보
\set VERBOSITY verbose
```

### C 테스트 디버깅
```bash
# GDB로 디버깅
gdb --args postgres
(gdb) break test_guc_run
(gdb) run
```

### 로그 확인
```bash
# PostgreSQL 로그 확인
tail -f /var/log/postgresql/postgresql-18-main.log

# 또는 설정 확인
psql -c "SHOW log_destination;"
psql -c "SHOW log_directory;"
```

---

## 트러블슈팅

### 문제: "extension does not exist"
```bash
# 해결: 확장 설치 확인
make install
psql -c "CREATE EXTENSION postfga_fdw;"
```

### 문제: "test_guc_run function not found"
```bash
# 해결: 함수 재생성
make test-unit
```

### 문제: pgTAP 테스트 실패
```bash
# 해결: pgTAP 설치 확인
psql -c "CREATE EXTENSION pgtap;"
```

---

## 모범 사례

1. **테스트 독립성**: 각 테스트는 독립적으로 실행 가능해야 함
2. **정리(Cleanup)**: 테스트 후 생성한 객체 삭제
3. **명확한 실패 메시지**: 테스트 실패 시 원인을 쉽게 파악
4. **경계값 테스트**: 최소/최대값, NULL, 빈 문자열 등
5. **트랜잭션 사용**: 테스트를 트랜잭션으로 감싸 롤백

---

## 참고 자료

- [PostgreSQL Extension Testing](https://www.postgresql.org/docs/current/extend-pgxs.html)
- [pgTAP Documentation](https://pgtap.org/)
- [PostgreSQL Regression Tests](https://www.postgresql.org/docs/current/regress.html)
