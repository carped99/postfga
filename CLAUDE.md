## 1. 프로젝트 개요 (요약)

* PostgreSQL 18 기준으로 작성하는 **FDW Extension + Background Worker** 프로젝트
* 언어: **C++20**
* 목적:

  * `acl_permission`이라는 **FDW 외부 테이블**을 통해 OpenFGA와 연동된 ACL 권한 조회를 수행
  * **Background Worker(BGW)** 가 gRPC로 OpenFGA의 변경 스트림/변경 API를 읽어와 **shared memory 캐시를 관리**
  * FDW는 shared memory 캐시를 우선 조회하고, 캐시 미스 시 OpenFGA gRPC를 직접 호출(옵션), 또는 miss로 처리
* 지원 PostgreSQL 버전

  * **기준: 18**
  * 하위 호환성: 18과 FDW/BGW API가 크게 달라지지 않은 버전 (예: 16, 17 정도)만 조건부 지원
  * 상위 버전(19+)은 **소스 코드/빌드 스위치만으로 쉽게 확장할 수 있도록** 설계 (버전 체크와 `#if PG_VERSION_NUM` 분기 최소화)
* 개발 환경

  * **Docker** 기반 개발 컨테이너
  * 컨테이너 안에서 PostgreSQL + dev header + gRPC + protobuf + 빌드 도구 설치
  * 호스트에서는 **VSCode**로 개발 (devcontainer 또는 `Remote - Containers` 사용)

---

## 2. 주요 컴포넌트 설계

### 2-1. FDW Extension 개요

1. **Extension 이름**

   * 예시: `openfga_fdw`
   * 생성 예:

     ```sql
     CREATE EXTENSION openfga_fdw;
     ```

2. **외부 테이블 스키마**

   FDW가 제공하는 기본 테이블: `acl_permission`

   ```sql
   CREATE FOREIGN TABLE acl_permission (
       object_type   text,
       object_id     text,
       subject_type  text,
       subject_id    text,
       relation      text
       -- 필요하다면 allowed boolean 같은 파생 컬럼을 추가해도 되지만,
       -- 우선은 "row가 있으면 allow" 로 해석하는 모델을 기본으로 한다.
   ) SERVER openfga_server OPTIONS (...);
   ```

3. **FDW 동작 모델**

   * 일반적인 사용 패턴:

     ```sql
     SELECT 1
       FROM acl_permission
      WHERE object_type  = 'document'
        AND object_id    = '123'
        AND subject_type = 'user'
        AND subject_id   = 'alice'
        AND relation     = 'read';
     ```
   * 결과:

     * **1 row 반환** → 허용(allow)
     * **0 row 반환** → 불허(deny)
   * 내부적으로는 위 5개 컬럼을 이용해 캐시 키를 만들고, 아래 로직 수행:

     1. shared memory 캐시에서 lookup
     2. 캐시 히트면 bitmask에서 해당 relation bit를 확인 (allow / deny 판단)
     3. 캐시 미스 or generation mismatch면

        * 옵션에 따라

          * (a) FDW가 직접 OpenFGA `Check` gRPC 호출 후 캐시 갱신
          * 또는 (b) 단순 miss → deny 처리
        * (스펙에서는 (a)를 기본으로, FDW 옵션으로 (b)도 선택 가능하게 설계)

4. **FDW 콜백 구현 (C API)**

   * `openfga_fdw_handler`:

     * 필수 FDW 콜백 등록

       * `GetForeignRelSize`
       * `GetForeignPaths`
       * `GetForeignPlan`
       * `BeginForeignScan`
       * `IterateForeignScan`
       * `EndForeignScan`
       * `ReScanForeignScan`
       * 필요 시 `AddForeignUpdateTargets`, `PlanForeignModify` 등은 **초기 버전에서는 미구현** (READ ONLY FDW)
   * `openfga_fdw_validator`:

     * 서버/테이블 옵션 검증

5. **FDW 서버/테이블 옵션 (예시)**

   서버 옵션 (`CREATE SERVER openfga_server OPTIONS (...)`):

   * `endpoint` : OpenFGA gRPC/HTTP endpoint (예: `dns:///openfga:8081`)
   * `store_id` : OpenFGA store ID
   * `authorization_model_id` (옵션)
   * `use_grpc` : `true/false`, 기본 true
   * `connect_timeout_ms`
   * `request_timeout_ms`

   테이블 옵션 (필요시):

   * `cache_ttl_ms` : 캐시 항목 TTL
   * `fallback_to_grpc_on_miss` : 캐시 미스 시 gRPC 콜 여부

   또는 위 옵션들을 GUC(`openfga_fdw.*`)로 노출해도 무방. (아래 GUC 섹션에서 구체화)

---

### 2-2. Background Worker(BGW) + Shared Memory 캐시

1. **BGW 역할**

   * PostgreSQL 서버 프로세스 내에서 **Background Worker**로 동작
   * OpenFGA의 변경 스트림을 gRPC로 구독/폴링

     * 예: `ReadChanges` API 또는 유사한 change log API
   * 변경 이벤트를 바탕으로 **shared memory의 캐시 및 generation 정보를 업데이트 / invalidate**

2. **Shared Memory 구조 (핵심 요구사항 반영)**

   요구사항:

   * shared memory에 캐시를 둔다.
   * **generation 기반 캐쉬 invalidate**
   * invalidate 단위:

     * `object_type`
     * `object_type + object_id`
     * `subject_type`
     * `subject_type + subject_id`
   * 캐시 데이터의 relation은 **bit 연산**으로 표현: `allow`, `deny` 저장
   * relation(문자열)을 bit로 변환해서 캐쉬에 사용
   * 여러 BGW 프로세스에서도 relation → bit 값이 **일관적**이어야 함

   제안 구조:

   ```c
   struct RelationBitMapEntry {
       NameData relation_name;      // relation 문자열 (예: "read", "write")
       uint8    bit_index;          // 0 ~ 63
   };

   struct AclCacheKey {
       // object_type:object_id / subject_type:subject_id
       NameData object_type;
       NameData object_id;       // 빈 문자열이면 "type-only" invalidation 대상
       NameData subject_type;
       NameData subject_id;      // 빈 문자열이면 "type-only" invalidation 대상
   };

   struct AclCacheEntry {
       AclCacheKey key;

       // generation 기반 invalidation
       uint64 object_type_gen;
       uint64 object_gen;
       uint64 subject_type_gen;
       uint64 subject_gen;

       // relation bitmask
       uint64 allow_bits;  // relation bit별 allow
       uint64 deny_bits;   // relation bit별 deny

       TimestampTz last_updated;
       TimestampTz expire_at;
   };

   struct AclSharedState {
       LWLock       *lock;           // hash table 보호용 LWLock
       HTAB         *relation_bitmap;// RelationBitMapEntry hash
       HTAB         *acl_cache;      // AclCacheEntry hash

       // generation counters (각 scope별)
       HTAB         *object_type_gen_map;  // key: object_type
       HTAB         *object_gen_map;       // key: object_type + object_id
       HTAB         *subject_type_gen_map; // key: subject_type
       HTAB         *subject_gen_map;      // key: subject_type + subject_id

       // 전역 설정 (예: TTL 등)도 여기에 저장 가능
   };
   ```

   * 모든 해시 테이블은 `ShmemInitHash`로 초기화
   * `AclSharedState`는 `ShmemInitStruct`로 초기화
   * 동시성 제어:

     * relation bitmap 업데이트 및 캐시 CRUD 시 `AclSharedState->lock` 사용
     * 필요시 read/write lock 분리도 가능하지만 첫 버전은 단일 LWLock으로 충분

3. **Generation 기반 invalidation 로직**

   * BGW는 OpenFGA의 변경 이벤트를 받아서:

     * 변경된 튜플에 포함된 `object_type`, `object_id`, `subject_type`, `subject_id` 를 해석
     * 아래 순서로 generation 증가:

       * 해당 `object_type`에 대한 generation++
       * 해당 `object_type+object_id` generation++
       * 해당 `subject_type` generation++
       * 해당 `subject_type+subject_id` generation++
   * FDW 스캔 시:

     1. 캐시 entry 조회
     2. entry 내에 저장된 `*_gen` 값과 shared memory의 현재 generation map 비교
     3. 하나라도 다르면 stale로 판단 → 캐시 삭제 또는 갱신 트리거
   * 캐시 갱신 시:

     * FDW 또는 BGW가 gRPC를 통해 fresh한 allow/deny 정보를 가져와 `AclCacheEntry` 갱신
     * 갱신 시점에 해당 키의 현재 generation을 다시 읽어 entry에 저장

4. **relation 문자열 → bit 매핑**

   요구사항:

   * relation(문자열)을 bit로 변환해서 캐쉬에 사용
   * 여러 BGW/백엔드 프로세스에서 bit값이 일관적이어야 함

   설계:

   * GUC로 relation 리스트를 정적 정의:

     * 예: `openfga_fdw.relations = 'read,write,edit,delete,download,owner'`
   * 서버 시작 시:

     * 위 문자열을 파싱하여 `RelationBitMapEntry` 해시 테이블에

       * 첫 번째 relation → bit 0
       * 두 번째 → bit 1
       * ...
     * 전체 relation 수는 최대 64개로 제한

       * 64개 초과 시

         * 서버 기동 시 에러를 내거나
         * 64개까지만 사용하고 나머지는 로그 경고 + 처리 불가로 설계 (선호 방식 선택)
   * 런타임에서 알 수 없는 relation이 들어오면:

     * 로그 경고를 남기고

       * (옵션1) 해당 relation은 always deny
       * (옵션2) gRPC는 호출하되 캐시에 저장하지 않음
   * 이 방식이면 **전 프로세스가 동일한 초기화 과정을 거쳐 bit index가 항상 동일**하게 된다.

5. **BGW에서 데이터 획득 방식**

   * BGW loop:

     1. 서버 start 시 `RegisterBackgroundWorker`로 등록
     2. `BgWorkerMain`에서

        * shared memory attach
        * gRPC 채널 생성 (OpenFGA)
        * `ReadChanges` 또는 유사 변경 API 호출
     3. 변경 이벤트 수신 시

        * event 파싱 → 변경된 tuple/object/subject 정보 확인
        * 위에서 정의한 generation map 갱신 및, 필요시 캐시 엔트리 직접 갱신
     4. 네트워크 에러/재연결 처리:

        * 백오프 전략과 재시도 로직 구현
        * 장애 시 초간단 fallback: 일정 시간 후 다시 connect

   * FDW가 직접 gRPC를 호출할지 여부는 옵션:

     * 기본: **FDW도 캐시 미스 시 gRPC `Check` 호출**
     * 고성능/안정성 필요시: BGW만 gRPC 호출하고 FDW는 캐시만 사용(미스=deny)

---

### 2-3. GUC 설정

아래와 같은 GUC를 정의:

* `openfga_fdw.endpoint` (string, SUSET)
* `openfga_fdw.store_id` (string, SUSET)
* `openfga_fdw.authorization_model_id` (string, SUSET, optional)
* `openfga_fdw.relations` (string, SUSET)

  * 예: `'read,write,edit,delete,download,owner'`
* `openfga_fdw.cache_ttl_ms` (int, default 예: 60000)
* `openfga_fdw.max_cache_entries` (int)
* `openfga_fdw.bgw_workers` (int, default 1)
* `openfga_fdw.fallback_to_grpc_on_miss` (bool, default true)

---

### 2-4. C++20 + PostgreSQL C API 사용 규칙

* `extern "C"` 로 PostgreSQL이 호출하는 entry-point(`_PG_init`, `_PG_fini`, handler, validator, bgw main 함수 등) 노출
* PostgreSQL의 메모리 관리 규칙 준수:

  * FDW 콜백 내에서 PostgreSQL 메모리 context 사용 (`palloc`, `pfree`)
  * shared memory 구조는 C struct 기반
* 예외 처리:

  * PostgreSQL 콜백 외부에서 C++ 예외 가능하지만,
  * 콜백 경계 바깥으로 예외가 던져지지 않도록 `try/catch`로 감싸고
  * 오류는 `ereport(ERROR, ...)` 로 변환
* 빌드:

  * C++20 표준 (`-std=c++20`)
  * gRPC C++ / protobuf 연동
  * PostgreSQL의 PGXS 빌드 시스템 사용 (Makefile 또는 CMake + PGXS wrapper)

---

## 3. Docker + 개발환경 + 배포

### 3-1. 개발용 Docker 환경

* 베이스 이미지: `postgres:18` (실제 태그는 나중에 결정)

* Dockerfile(dev):

  * 설치 항목:

    * `postgresql-server-dev-18`
    * `build-essential`, `cmake`, `ninja-build`
    * `protobuf-compiler`, `libprotobuf-dev`
    * `grpc` / `grpc-dev` / `libgrpc++-dev` (배포판 패키지 or 소스 빌드)
    * `git`, `vim`, `gdb` 등
  * 소스 디렉터리를 `/workspace`에 마운트하여 `make USE_PGXS=1` 또는 `cmake --build` 수행 가능하게

* VSCode 설정:

  * `.devcontainer/devcontainer.json` 작성

    * 위 Dockerfile을 사용
    * 포트 5432 노출
    * 확장: C/C++, CMake Tools, clangd 등 설치
  * `c_cpp_properties.json`:

    * includePath에 `/usr/include/postgresql/18/server`, `/usr/include/postgresql/internal` 포함
    * `compilerPath`: `/usr/bin/g++`

### 3-2. 배포용 Docker 이미지

* 멀티스테이지 Dockerfile:

  ```dockerfile
  FROM postgres:18 AS builder

  RUN apt-get update && apt-get install -y \
      postgresql-server-dev-18 build-essential cmake ninja-build \
      protobuf-compiler libprotobuf-dev libgrpc++-dev git \
      && rm -rf /var/lib/apt/lists/*

  WORKDIR /src
  COPY . .
  RUN make USE_PGXS=1

  FROM postgres:18

  # extension 바이너리 및 control/sql 복사
  COPY --from=builder /usr/lib/postgresql/18/lib/openfga_fdw.so /usr/lib/postgresql/18/lib/
  COPY --from=builder /usr/share/postgresql/18/extension/openfga_fdw.control /usr/share/postgresql/18/extension/
  COPY --from=builder /usr/share/postgresql/18/extension/openfga_fdw--*.sql /usr/share/postgresql/18/extension/

  # 필요하면 initdb 스크립트 추가 (자동 CREATE EXTENSION)
  COPY docker-initdb.d/ /docker-entrypoint-initdb.d/
  ```

* 사용자는:

  * Docker Hub에 올라간 이미지 사용
  * `docker run -e POSTGRES_PASSWORD=... your-dockerhub-user/postgres-openfga-fdw:latest`

### 3-3. 바이너리 배포

* GitHub 또는 GitLab Releases에 다음 내용 포함:

  * `openfga_fdw-<version>-pg18-linux-x86_64.tar.gz` 등

    * `lib/openfga_fdw.so`
    * `share/extension/openfga_fdw.control`
    * `share/extension/openfga_fdw--<version>.sql`

* 사용자는 tarball을 풀어서 해당 경로에 복사 후 `CREATE EXTENSION openfga_fdw;` 로 설치

---

## 4. PostgreSQL 버전 대응

* `#include "pg_config.h"` 후 `PG_VERSION_NUM` 사용
* 최소 요구:

  ```c
  #if PG_VERSION_NUM < 180000
  #error "openfga_fdw requires PostgreSQL 18.0 or later"
  #endif
  ```
* FDW/BGW API 차이가 거의 없는 18, 19 등은 별도 분기가 필요할 때만 `#if PG_VERSION_NUM >= 190000` 식으로 처리

---

## 5. Claude에게 줄 최종 프롬프트 (복붙용)

아래 블록을 Claude에게 그대로 붙여넣고
“이 스펙대로 초기 골격부터 구현해 줘. Makefile/PGXS, Dockerfile, C++ 코드(헤더/소스) 구조까지 만들어 줘.”
라고 요청하면 됩니다.

```text
PostgreSQL FDW Extension + Background Worker를 C++20으로 구현하는 프로젝트의 전체 스펙을 정리한다. 아래 요구사항을 모두 만족하는 초기 구현(프로젝트 스캐폴딩 + 핵심 코드 골격)을 작성해 달라.

[목표]
- PostgreSQL 18 기준으로 동작하는 FDW Extension + Background Worker를 C++20으로 구현한다.
- 외부 테이블 `acl_permission`을 통해 OpenFGA와 연동된 ACL 권한 체크를 수행한다.
- shared memory 기반 캐시와 generation 방식 invalidation을 적용한다.
- BGW는 OpenFGA의 변경 스트림을 gRPC로 받아서 shared memory 캐시 및 generation 정보를 갱신한다.
- Docker 기반 개발/빌드 환경, VSCode(devcontainer), 배포용 Docker 이미지, 바이너리 릴리즈 구조까지 설계한다.

[기술 스택 및 제약]
- PostgreSQL: 18 기준, 하위 호환은 18 기준으로 FDW/BGW API 변동이 적은 버전만 지원. 상위 버전(19+)도 확장 가능하게 설계.
- 언어: C++20
- 빌드: PostgreSQL PGXS 기반 (Makefile 필수). 필요하면 CMake + PGXS 래퍼도 허용.
- gRPC + protobuf C++ 클라이언트 사용(OpenFGA와 통신).
- 예외가 PostgreSQL C 콜백 경계를 넘어가지 않도록 try/catch로 감싸고 ereport(ERROR, ...)로 변환.

[FDW 설계]
1. Extension 이름: `openfga_fdw`
   - entrypoints:
     - `_PG_init`, `_PG_fini`
     - `openfga_fdw_handler`
     - `openfga_fdw_validator`

2. FDW 서버 및 테이블
   - 예시 서버:
     CREATE SERVER openfga_server FOREIGN DATA WRAPPER openfga_fdw OPTIONS (...);
   - 외부 테이블:
     CREATE FOREIGN TABLE acl_permission (
         object_type   text,
         object_id     text,
         subject_type  text,
         subject_id    text,
         relation      text
     ) SERVER openfga_server OPTIONS (...);

3. FDW 동작 모델
   - 사용 예:
     SELECT 1 FROM acl_permission
      WHERE object_type  = 'document'
        AND object_id    = '123'
        AND subject_type = 'user'
        AND subject_id   = 'alice'
        AND relation     = 'read';
   - 행이 1개 반환되면 allow, 0개면 deny로 해석.
   - 내부 동작:
     1) 위 5개 컬럼으로 캐시 키 구성.
     2) shared memory 캐시에서 lookup.
     3) 히트하면 relation bitmask에서 해당 relation bit(allow/deny)를 확인하고, allow이면 1 row를 반환, deny이면 0 row 반환.
     4) 캐시 miss 또는 generation mismatch면:
        - 옵션에 따라:
          - a) FDW가 OpenFGA Check gRPC를 호출해서 fresh한 결과를 가져오고 캐시를 갱신 후, 결과에 따라 row 반환.
          - b) gRPC를 호출하지 않고 단순 miss로 간주(deny) – 이 모드는 설정값으로 제어.

4. FDW 필수 콜백 구현
   - GetForeignRelSize, GetForeignPaths, GetForeignPlan
   - BeginForeignScan, IterateForeignScan, ReScanForeignScan, EndForeignScan
   - READ ONLY FDW로 시작하므로 DML 관련 콜백(PlanForeignModify 등)은 구현하지 않거나 ERROR 반환.

5. FDW 옵션 및/또는 GUC
   - 서버 옵션 예:
     - endpoint: OpenFGA gRPC endpoint (예: "dns:///openfga:8081")
     - store_id: OpenFGA store ID
     - authorization_model_id: optional
     - use_grpc: true/false (기본 true)
     - connect_timeout_ms
     - request_timeout_ms
   - 전역 GUC 예 (SUSET):
     - openfga_fdw.endpoint (string)
     - openfga_fdw.store_id (string)
     - openfga_fdw.authorization_model_id (string)
     - openfga_fdw.relations (string, 예: "read,write,edit,delete,download,owner")
     - openfga_fdw.cache_ttl_ms (int, default 60000)
     - openfga_fdw.max_cache_entries (int)
     - openfga_fdw.bgw_workers (int, default 1)
     - openfga_fdw.fallback_to_grpc_on_miss (bool, default true)

[Background Worker + Shared Memory 캐시 설계]
1. BGW 개요
   - PostgreSQL 서버에서 BGW로 실행.
   - OpenFGA 변경 스트림(ReadChanges 등)을 gRPC로 받아서 캐시 무효화 및 필요시 캐시 갱신.
   - RegisterBackgroundWorker를 통해 등록되고, BgWorkerMain을 통해 실행.

2. Shared Memory 구조
   - 아래와 같은 C struct 기반 설계:

     struct RelationBitMapEntry {
         NameData relation_name;
         uint8    bit_index;   // 0~63
     };

     struct AclCacheKey {
         NameData object_type;
         NameData object_id;       // 빈 문자열이면 type-only scope
         NameData subject_type;
         NameData subject_id;      // 빈 문자열이면 type-only scope
     };

     struct AclCacheEntry {
         AclCacheKey key;

         uint64 object_type_gen;
         uint64 object_gen;
         uint64 subject_type_gen;
         uint64 subject_gen;

         uint64 allow_bits;
         uint64 deny_bits;

         TimestampTz last_updated;
         TimestampTz expire_at;
     };

     struct AclSharedState {
         LWLock       *lock;
         HTAB         *relation_bitmap;       // RelationBitMapEntry hash
         HTAB         *acl_cache;            // AclCacheEntry hash

         HTAB         *object_type_gen_map;  // key: object_type
         HTAB         *object_gen_map;       // key: object_type + object_id
         HTAB         *subject_type_gen_map; // key: subject_type
         HTAB         *subject_gen_map;      // key: subject_type + subject_id
     };

   - 모든 해시는 ShmemInitHash로 초기화, AclSharedState는 ShmemInitStruct로 초기화.
   - 동시성 제어는 AclSharedState->lock(LWLock)로 처리.

3. Generation 기반 invalidation
   - BGW가 OpenFGA 변경 이벤트를 받을 때마다:
     - event에 포함된 object_type, object_id, subject_type, subject_id를 해석.
     - 해당하는 generation 카운터를 증가:
       - object_type
       - object_type+object_id
       - subject_type
       - subject_type+subject_id
   - FDW에서 캐시 조회 시:
     1) 캐시 엔트리에서 object_type_gen, object_gen, subject_type_gen, subject_gen 값을 읽는다.
     2) shared memory의 현재 generation map과 비교한다.
     3) 어느 하나라도 다르면 stale로 판단하고 엔트리 삭제 또는 재조회(gRPC 호출) 후 갱신.

4. relation 문자열 → bit 매핑
   - GUC openfga_fdw.relations 에 relation 리스트를 정의: e.g. "read,write,edit,delete,download,owner"
   - 서버 시작 시:
     - 이 문자열을 파싱하여 RelationBitMapEntry hash에 등록.
     - 첫 번째 relation → bit_index 0, 두 번째 → 1, ...
     - relation 개수는 64 이하로 제한.
   - 런타임에 캐시 혹은 gRPC 결과에서 relation 문자열을 사용할 때:
     - relation_bitmap에서 bit_index를 찾고, 해당 bit를 allow_bits/deny_bits에 설정 또는 검사.
   - 알 수 없는 relation이면:
     - WARNING 로그를 남기고, 해당 relation은 캐시하지 않거나 항상 deny로 처리하는 정책을 명확히 코드에 표현.

5. BGW 로직
   - BgWorkerMain 개요:
     - shared memory attach
     - GUC에서 endpoint, store_id, relations 등을 읽어 relation_bitmap이 초기화 되어 있다고 가정하거나 필요시 재초기화
     - gRPC 채널 생성
     - 무한 loop:
       - OpenFGA ReadChanges(또는 유사 API) 호출/대기
       - 이벤트 수신 시, 해당 object/subject에 대한 generation map을 갱신
       - 필요하면 특정 key에 대한 캐시 엔트리를 미리 다시 채우는 프리페치 로직도 확장 가능
       - 네트워크 에러 시 재연결/backoff

[FDW와 BGW의 관계]
- FDW는 shared memory 캐시를 읽고, 캐시 miss 또는 generation mismatch 시:
  - 옵션에 따라 직접 OpenFGA Check를 호출하여 캐시를 갱신하거나,
  - miss를 deny로 처리.
- BGW는 주로 변경 이벤트를 처리하고 generation을 증가시켜 invalidate를 트리거한다.

[PostgreSQL 버전 처리]
- pg_config.h의 PG_VERSION_NUM을 사용해서 최소 버전을 18로 고정:
  #if PG_VERSION_NUM < 180000
  #error "openfga_fdw requires PostgreSQL 18.0 or later"
  #endif
- 향후 19 이상에서 FDW/BGW API가 변경될 경우를 대비해, 필요한 부분에만 #if PG_VERSION_NUM 조건부 분기 사용.

[Docker 개발환경]
1) dev용 Dockerfile
   - FROM postgres:18
   - apt-get으로
     - postgresql-server-dev-18
     - build-essential, cmake, ninja-build
     - protobuf-compiler, libprotobuf-dev, libgrpc++-dev
     - git, gdb 등 설치
   - WORKDIR /workspace
   - 소스를 마운트한 상태에서 `make USE_PGXS=1`로 빌드 가능하게 구성.

2) VSCode devcontainer
   - .devcontainer/devcontainer.json 작성:
     - 위 Dockerfile 사용
     - 포트 5432
     - 확장: ms-vscode.cpptools, ms-vscode.cmake-tools 등
   - .vscode/c_cpp_properties.json:
     - includePath: /usr/include/postgresql/18/server, /usr/include/postgresql/internal 추가
     - compilerPath: /usr/bin/g++

[배포용 Docker 이미지]
- 멀티 스테이지 Dockerfile:
  - 1단계(builder): postgres:18 이미지에서 dev 패키지를 설치하고 make USE_PGXS=1로 빌드.
  - 2단계(runtime): postgres:18 이미지에 .so, control, sql 파일을 복사.
  - /docker-entrypoint-initdb.d/ 아래에 init 스크립트를 추가해서 자동 CREATE EXTENSION openfga_fdw 를 수행할 수도 있음.
- 결과 이미지를 Docker Hub에 푸시할 수 있도록 Dockerfile을 구조화.

[바이너리 릴리즈]
- GitHub/GitLab Releases에
  - openfga_fdw-<version>-pg18-linux-x86_64.tar.gz 등으로 패키징:
    - lib/openfga_fdw.so
    - share/extension/openfga_fdw.control
    - share/extension/openfga_fdw--<version>.sql
- 사용자는 해당 tarball을 받아서 PostgreSQL 서버의 적절한 경로에 복사 후 CREATE EXTENSION openfga_fdw; 실행.

[요청 사항]
- 위 전체 스펙에 맞게:
  1) 디렉터리 구조 제안 (src/, include/, sql/, docker/, .devcontainer/ 등).
  2) PGXS 기반 Makefile 작성.
  3) C++20 코드 골격:
     - FDW handler/validator
     - shared memory 초기화 코드
     - BGW 등록 코드와 BgWorkerMain 골격
     - gRPC 클라이언트 wrapper 클래스 인터페이스 (구현은 stub 형태라도 좋음)
     - 캐시 조회/갱신 유틸리티 함수 골격
  4) dev용 Dockerfile, 배포용 멀티 스테이지 Dockerfile 예시.
  5) VSCode devcontainer 설정 예시 파일.

- 실제 gRPC 메시지 스키마(OpenFGA protobuf)는 간략한 가짜 인터페이스/타입으로 대체해도 좋다. 하지만 전체 구조가 나중에 실제 OpenFGA proto로 교체하기 쉽게 설계해 달라.
```
