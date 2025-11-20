# PostFGA GUC 테스트 코드 요약

## 생성된 파일 목록

```
tests/
├── Makefile                    # 테스트 빌드 및 실행 자동화
├── README_TESTING.md           # 상세 테스트 가이드
├── TESTING_SUMMARY.md          # 이 파일
├── run_tests.sh                # 테스트 실행 스크립트
├── test_helpers.h              # 공통 테스트 유틸리티
├── test_guc.c                  # C 단위 테스트
└── sql/
    ├── guc_test.sql            # SQL 회귀 테스트
    └── guc_pgtap_test.sql      # pgTAP 테스트
```

---

## 빠른 시작

### 1. SQL 테스트 (가장 간단)
```bash
cd tests
./run_tests.sh sql

# 또는 직접 실행
psql -d postgres -f sql/guc_test.sql
```

### 2. 전체 테스트
```bash
cd tests
./run_tests.sh
```

---

## 각 테스트 파일 설명

### 1. `sql/guc_test.sql` - SQL 회귀 테스트
**목적**: PostgreSQL 표준 테스트 방식
**테스트 내용**:
- ✓ GUC 변수 기본값 확인
- ✓ 값 변경 및 조회
- ✓ 경계값 테스트 (최소/최대)
- ✓ 불린 값 테스트
- ✓ 잘못된 값 거부 확인
- ✓ RESET 동작 확인
- ✓ 여러 개의 relations 테스트

**실행**:
```bash
psql -d postgres -f sql/guc_test.sql
```

**출력 예시**:
```
openfga.endpoint
-----------------------------
dns:///localhost:8081

SET
openfga.cache_ttl_ms
----------------------
30000
```

---

### 2. `test_guc.c` - C 단위 테스트
**목적**: C 함수 레벨의 정밀한 테스트
**테스트 내용**:
- ✓ Config 초기화 확인
- ✓ 값 범위 검증
- ✓ 문자열 설정 확인
- ✓ 불린 설정 확인
- ✓ 숫자 설정 확인

**빌드 및 실행**:
```bash
# 빌드
cd tests
make test_guc.so

# PostgreSQL에서 실행
psql -d postgres <<SQL
CREATE FUNCTION test_guc_run() RETURNS BOOLEAN
AS '$(pwd)/test_guc', 'test_guc_run'
LANGUAGE C STRICT;

SELECT test_guc_run();
SQL
```

**출력 예시**:
```
[TEST 1] Config Initialization
  ✓ PASS: Config pointer should not be NULL
  ✓ PASS: Default endpoint should be set

Test Summary
=================================================
Total Tests:  5
Passed:       12
Failed:       0
✓ All tests passed!
```

---

### 3. `sql/guc_pgtap_test.sql` - pgTAP 테스트
**목적**: TAP 프로토콜 기반 고급 테스트
**테스트 내용**:
- ✓ GUC 변수 존재 확인
- ✓ 기본값 검증
- ✓ 값 변경 동작
- ✓ 경계값 및 에러 케이스
- ✓ Relations 파싱

**실행**:
```bash
# pgTAP가 설치된 경우
pg_prove -d postgres sql/guc_pgtap_test.sql

# 또는 직접 실행
psql -d postgres -f sql/guc_pgtap_test.sql
```

---

### 4. `test_helpers.h` - 테스트 유틸리티
**목적**: 재사용 가능한 테스트 매크로 제공

**주요 매크로**:
```c
TEST_SUITE_START(name)              // 테스트 스위트 시작
TEST_START(name)                     // 개별 테스트 시작
TEST_ASSERT(condition, msg)          // 기본 assertion
TEST_ASSERT_STR_EQ(a, b, msg)       // 문자열 비교
TEST_ASSERT_INT_EQ(a, b, msg)       // 정수 비교
TEST_ASSERT_BOOL_EQ(a, b, msg)      // 불린 비교
TEST_ASSERT_NOT_NULL(ptr, msg)      // NULL 아님 확인
TEST_SUITE_END()                     // 결과 출력
```

**사용 예**:
```c
#include "test_helpers.h"

void my_test() {
    TEST_SUITE_START("My Test Suite");

    TEST_START("Test addition");
    int result = add(2, 3);
    TEST_ASSERT_INT_EQ(result, 5, "2 + 3 should equal 5");

    TEST_SUITE_END();
}
```

---

### 5. `run_tests.sh` - 테스트 실행 스크립트
**목적**: 모든 테스트를 편리하게 실행

**사용법**:
```bash
./run_tests.sh          # 모든 테스트
./run_tests.sh sql      # SQL 테스트만
./run_tests.sh unit     # Unit 테스트만
./run_tests.sh pgtap    # pgTAP 테스트만
```

**환경 변수**:
```bash
PGHOST=localhost \
PGPORT=5432 \
PGDATABASE=testdb \
PGUSER=postgres \
./run_tests.sh
```

---

### 6. `Makefile` - 빌드 자동화
**타겟**:
```bash
make test           # 모든 테스트
make test-sql       # SQL 테스트
make test-unit      # Unit 테스트
make test-pgtap     # pgTAP 테스트
make clean-test     # 정리
make help           # 도움말
```

---

## C 테스트 작성 패턴

### 기본 패턴
```c
#include "test_helpers.h"
#include "../src/my_module.h"

static void test_my_feature(void)
{
    TEST_START("My Feature Test");

    // Arrange (준비)
    int input = 42;

    // Act (실행)
    int result = my_function(input);

    // Assert (검증)
    TEST_ASSERT_INT_EQ(result, 42, "Should return input value");
}

static void run_all_tests(void)
{
    TEST_SUITE_START("My Module Tests");
    test_my_feature();
    TEST_SUITE_END();
}
```

### PostgreSQL Extension으로 실행
```c
PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(my_test_run);

Datum
my_test_run(PG_FUNCTION_ARGS)
{
    run_all_tests();
    PG_RETURN_BOOL(TEST_RESULT() == 0);
}
```

---

## SQL 테스트 작성 패턴

### 기본 패턴
```sql
-- Test 1: 기본값 확인
SHOW my_guc_variable;

-- Test 2: 값 변경
SET my_guc_variable = 'new_value';
SHOW my_guc_variable;

-- Test 3: 에러 케이스
DO $$
BEGIN
    EXECUTE 'SET my_guc_variable = invalid_value';
    RAISE EXCEPTION 'Should have failed';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Correctly rejected invalid value';
END $$;

-- Test 4: 정리
RESET my_guc_variable;
```

---

## pgTAP 테스트 작성 패턴

### 기본 패턴
```sql
BEGIN;
SELECT plan(5);

-- Test 1: 함수 존재
SELECT has_function('my_function');

-- Test 2: 반환 타입 확인
SELECT function_returns('my_function', 'integer');

-- Test 3: 결과 검증
SELECT is(
    my_function(42),
    42,
    'Function should return correct value'
);

-- Test 4: GUC 설정 확인
SELECT is(
    current_setting('my_guc'),
    'expected_value',
    'GUC should have correct default'
);

-- Test 5: 에러 케이스
SELECT throws_ok(
    'SELECT my_function(NULL)',
    'My error message'
);

SELECT * FROM finish();
ROLLBACK;
```

---

## 디버깅 팁

### SQL 테스트 디버깅
```bash
# Verbose 모드
psql -d postgres -f sql/guc_test.sql -e

# 에러 상세 정보
psql -d postgres -v ON_ERROR_STOP=1 -f sql/guc_test.sql
```

### C 테스트 디버깅
```bash
# 컴파일 경고 활성화
make CFLAGS="-Wall -Wextra -g" test_guc.so

# GDB로 디버깅
gdb postgres
(gdb) break test_guc_run
(gdb) run
```

### PostgreSQL 로그 확인
```bash
# 로그 레벨 설정
psql -c "SET client_min_messages = DEBUG1;"

# 로그 파일 확인
tail -f /var/log/postgresql/postgresql-*.log
```

---

## CI/CD 통합

### GitHub Actions
```yaml
- name: Run tests
  run: |
    cd tests
    ./run_tests.sh
  env:
    PGHOST: localhost
    PGUSER: postgres
    PGDATABASE: postgres
```

### GitLab CI
```yaml
test:
  script:
    - cd tests
    - ./run_tests.sh
```

---

## 참고 자료

- **PostgreSQL Testing**: https://www.postgresql.org/docs/current/regress.html
- **pgTAP**: https://pgtap.org/
- **PostgreSQL Extension Development**: https://www.postgresql.org/docs/current/extend.html

---

## 문제 해결

### "extension does not exist"
```bash
make install
psql -c "CREATE EXTENSION postfga_fdw;"
```

### "pg_config not found"
```bash
export PATH=/usr/lib/postgresql/18/bin:$PATH
# 또는
make PG_CONFIG=/path/to/pg_config
```

### "permission denied"
```bash
sudo make install
# 또는 개발 환경에서
make USE_PGXS=1 install
```
