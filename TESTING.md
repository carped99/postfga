# PostFGA Testing Guide

이 문서는 PostFGA extension의 테스트 방법을 설명합니다.

## 테스트 구성

### 1. 독립 실행형 클라이언트 테스트 (test_client)

gRPC 클라이언트(`client.cpp`)를 테스트하는 독립 실행형 프로그램입니다.

**빌드:**
```bash
cd /workspace/build
cmake -DBUILD_CLIENT_TEST=ON ..
cmake --build . --target test_client
```

**실행:**
```bash
./test_client [endpoint]
# 기본 endpoint: localhost:8080
```

**기능:**
- 매 1초마다 async CHECK 요청 전송
- 매 5초마다 sync CHECK 요청 전송  
- 콜백 결과 출력
- Ctrl+C로 종료

**출력 예시:**
```
PostFGA Client Test
===================
Endpoint: localhost:8080
Client initialized successfully

[Request #1] Sending async check: document:budget#reader@user:alice
[Request #2] Sending async check: document:budget#reader@user:alice
[Callback #1] allowed=1, error_code=0, error_message=''
[Request #3] Sending async check: document:budget#reader@user:alice
[Callback #2] allowed=1, error_code=0, error_message=''
```

---

### 2. PostgreSQL Extension 테스트 (test_queue.c)

PostgreSQL extension 내부에서 request queue를 테스트하는 함수들입니다.

**빌드:**
```bash
cd /workspace/build
cmake --build .
cmake --install .
```

**PostgreSQL 재시작:**
```bash
# PostgreSQL 설정 파일에 shared_preload_libraries 추가
echo "shared_preload_libraries = 'postfga'" >> /tmp/pgdata/postgresql.conf

# PostgreSQL 재시작
pg_ctl -D /tmp/pgdata -l /tmp/pgdata/logfile restart
```

**테스트 SQL 실행:**
```bash
psql -U postgres < /workspace/sql/test_queue.sql
```

**또는 대화형으로:**
```sql
-- 큐 상태 확인
SELECT fga_test_queue_stats();

-- 10개 요청 한번에 추가
SELECT fga_test_enqueue(10);

-- 1초에 하나씩 10개 요청 추가 (10초 소요)
SELECT fga_test_enqueue_continuous();

-- 완료된 요청 정리
SELECT fga_test_clear_queue();
```

---

## 테스트 시나리오

### 시나리오 1: 독립 클라이언트만 테스트

OpenFGA 서버 필요:

```bash
# Terminal 1: OpenFGA 실행
docker run -p 8080:8080 -p 8081:8081 openfga/openfga run

# Terminal 2: 테스트 실행
cd /workspace/build
./test_client localhost:8080
```

### 시나리오 2: PostgreSQL Extension 테스트

```bash
# Terminal 1: PostgreSQL 재시작
pg_ctl -D /tmp/pgdata -l /tmp/pgdata/logfile restart

# Terminal 2: 테스트 SQL 실행
psql -U postgres -f /workspace/sql/test_queue.sql

# Terminal 3: 로그 모니터링
tail -f /tmp/pgdata/logfile | grep PostFGA
```

### 시나리오 3: 통합 테스트 (Full Stack)

전체 흐름 테스트:

```bash
# Terminal 1: OpenFGA
docker run -p 8080:8080 -p 8081:8081 openfga/openfga run

# Terminal 2: PostgreSQL 시작
pg_ctl -D /tmp/pgdata -l /tmp/pgdata/logfile start

# Terminal 3: Extension 내부에서 요청 생성
psql -U postgres -c "SELECT fga_test_enqueue_continuous();"

# Terminal 4: BGW 로그 확인
tail -f /tmp/pgdata/logfile | grep "PostFGA BGW"
```

예상 로그:
```
PostFGA Test: [1/10] Enqueued request #1
PostFGA BGW: Processing 1 requests
PostFGA BGW: Request 1 completed: allowed=1
PostFGA Test: [2/10] Enqueued request #2
...
```

---

## 문제 해결

### test_client 연결 실패

```
Error: Failed to initialize gRPC client
```

**해결:** OpenFGA 서버가 실행 중인지 확인
```bash
docker ps | grep openfga
curl http://localhost:8080/healthz
```

### test_queue 함수를 찾을 수 없음

```
ERROR:  function fga_test_enqueue(integer) does not exist
```

**해결:** Extension 재빌드 및 재설치 필요
```bash
cd /workspace/build
cmake --build . && cmake --install .
pg_ctl -D /tmp/pgdata -l /tmp/pgdata/logfile restart
```

### BGW가 요청을 처리하지 않음

**확인사항:**
1. BGW가 실행 중인지 확인:
   ```sql
   SELECT * FROM pg_stat_activity WHERE backend_type = 'fga_worker';
   ```

2. GUC 설정 확인:
   ```sql
   SHOW postfga.endpoint;
   SHOW postfga.store_id;
   ```

3. 로그 확인:
   ```bash
   grep "PostFGA BGW" /tmp/pgdata/logfile
   ```

---

## 성능 테스트

### 대량 요청 테스트

```sql
-- 256개 요청 (큐 최대 크기)
SELECT fga_test_enqueue(256);
SELECT fga_test_queue_stats();
```

### 동시성 테스트

```bash
# 여러 psql 세션에서 동시 실행
for i in {1..5}; do
  psql -U postgres -c "SELECT fga_test_enqueue(50);" &
done
wait
```

---

## 다음 단계

테스트가 성공하면:
1. FDW 테이블 구현 (`acl_permission`)
2. FDW에서 큐 사용
3. BGW에서 큐 처리 및 결과 반환
4. Latch 기반 대기/깨우기 구현

