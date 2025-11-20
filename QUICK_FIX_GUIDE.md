# Quick Fix Guide - undefined symbol: init_shared_memory

## 문제
```
/usr/lib/postgresql/18/lib/postfga.so: undefined symbol: init_shared_memory
```

## 원인
`shared_memory.c` 파일이 없어서 `init_shared_memory()` 함수가 구현되지 않았습니다.

## 해결 완료

### 1. ✅ `src/shared_memory.c` 파일 생성
전체 shared memory 관리 구현이 포함된 파일이 생성되었습니다.

### 2. ✅ `CMakeLists.txt` 업데이트
`shared_memory.c`가 빌드 소스에 추가되었습니다:

```cmake
set(EXTENSION_SOURCES
    src/init.c
    src/shared_memory.c    # ← 추가됨
    src/postfga_bgw.cpp
)
```

## 빌드 방법

### Devcontainer 사용 (권장)

```bash
# 1. VSCode에서 devcontainer 열기
# 2. 터미널에서 다음 명령 실행:

cd /workspace
rm -rf build  # 기존 빌드 디렉터리 삭제
mkdir build
cd build
cmake ..
cmake --build .

# 3. 설치 및 테스트
cd /workspace
./scripts/install_and_test.sh
```

### Windows에서 직접 빌드 (Docker Desktop 필요)

```bash
# PowerShell 또는 CMD에서:
cd C:\Users\tykim\repos\postfga

# Docker를 통해 빌드
docker run --rm -v ${PWD}:/workspace -w /workspace postgres:18 bash -c "
  apt-get update &&
  apt-get install -y build-essential cmake postgresql-server-dev-18 &&
  cd /workspace &&
  rm -rf build &&
  mkdir build &&
  cd build &&
  cmake .. &&
  cmake --build .
"
```

## 확인 방법

빌드 후 다음 명령으로 확인:

```bash
# shared_memory.o 파일이 생성되었는지 확인
ls build/CMakeFiles/postfga.dir/src/

# postfga.so에 init_shared_memory 심볼이 있는지 확인
nm build/postfga.so | grep init_shared_memory

# 출력 예상:
# 00000000000xxxxx T init_shared_memory
```

## 구현된 기능

`src/shared_memory.c`에는 다음 함수들이 구현되었습니다:

- ✅ `init_shared_memory()` - Shared memory 초기화
- ✅ `get_shared_state()` - Shared state 포인터 획득
- ✅ `cleanup_shared_memory()` - Cleanup (placeholder)
- ✅ `cache_lookup()` - 캐시 조회
- ✅ `cache_insert()` - 캐시 삽입
- ✅ `cache_delete()` - 캐시 삭제
- ✅ `get_generation()` - Generation 조회
- ✅ `increment_generation()` - Generation 증가
- ✅ `is_cache_stale()` - 캐시 유효성 검사
- ✅ `get_relation_bit_index()` - Relation bit index 조회
- ✅ `register_relation()` - Relation 등록
- ⏳ `enqueue_grpc_request()` - gRPC 요청 큐 (stub)
- ⏳ `dequeue_grpc_requests()` - gRPC 요청 dequeue (stub)
- ⏳ `wait_for_grpc_result()` - 결과 대기 (stub)
- ⏳ `set_grpc_result()` - 결과 설정 (stub)
- ⏳ `cache_invalidate_by_scope()` - Scope 기반 invalidation (stub)
- ⏳ `set_bgw_latch()` - BGW latch 설정 (stub)
- ⏳ `notify_bgw_of_pending_work()` - BGW 통지 (stub)

## 다음 단계

1. **빌드 및 설치**
   ```bash
   ./scripts/install_and_test.sh
   ```

2. **확장 생성 테스트**
   ```sql
   CREATE EXTENSION postfga;
   SELECT * FROM pg_extension WHERE extname = 'postfga';
   ```

3. **캐시 함수 테스트**
   ```sql
   SELECT postfga_fdw.cache_stats();
   SELECT postfga_fdw.clear_cache();
   ```

## 트러블슈팅

### 여전히 "undefined symbol" 오류가 발생하는 경우

1. **완전히 새로 빌드**:
   ```bash
   rm -rf build
   mkdir build
   cd build
   cmake ..
   cmake --build . --clean-first
   ```

2. **이전 설치 제거**:
   ```bash
   sudo rm -f $(pg_config --pkglibdir)/postfga.so
   sudo rm -f $(pg_config --sharedir)/extension/postfga*
   ```

3. **재설치**:
   ```bash
   ./scripts/install_extension.sh
   ```

4. **PostgreSQL 재시작** (필요한 경우):
   ```bash
   sudo systemctl restart postgresql
   # 또는
   sudo service postgresql restart
   ```

## 참고

- 모든 변경사항이 git에 커밋되지 않았을 수 있습니다
- 빌드 전에 최신 코드인지 확인하세요
- devcontainer 사용을 강력히 권장합니다 (일관된 빌드 환경)
