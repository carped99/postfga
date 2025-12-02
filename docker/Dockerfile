# ------------------------------------------------------------
# 1단계: 개발용 이미지
# ------------------------------------------------------------
ARG PG_MAJOR=18
FROM postgis/postgis:${PG_MAJOR}-3.6 AS dev-builder

# Install build toolchain and headers
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    gdb cmake git autoconf libtool pkg-config bear \
    libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc \
    libxxhash-dev \
    postgresql-server-dev-${PG_MAJOR} \
    nodejs npm \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for VSCode
RUN useradd -ms /bin/bash vscode

WORKDIR /workspace

# 1) 의존성 정의 파일만 복사 (CMakeLists, cmake/ 모듈, FetchContent 설정 등)
#COPY CMakeLists.txt cmake/ ./
# 2) 먼저 configure를 한 번 해서 FetchContent로 deps만 받아두기
#RUN cmake -S . -B build

# ------------------------------------------------------------
# 2단계: 빌드용 이미지
# ------------------------------------------------------------
FROM dev-builder AS release-builder
WORKDIR /workspace
COPY . .
RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release

RUN cmake --build build --config Release

RUN DESTDIR=/out cmake --install build

# ------------------------------------------------------------
# 2단계: 설치용 이미지
# ------------------------------------------------------------
FROM postgis/postgis:${PG_MAJOR}-3.6 AS runtime

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      libgrpc++1 \
      libprotobuf32 \
      && rm -rf /var/lib/apt/lists/*

# 1단계에서 설치된 파일들을 복사
# 경로는 PGXS install 결과를 보고 맞춰야 함
COPY --from=release-builder /out/usr/lib/postgresql/${PG_MAJOR}/lib/*.so \
    /usr/lib/postgresql/${PG_MAJOR}/lib/

COPY --from=release-builder /out/usr/share/postgresql/${PG_MAJOR}/extension/* \
    /usr/share/postgresql/${PG_MAJOR}/extension/

# 필요하다면 기본 설정도 같이 넣을 수 있음
# 예: shared_preload_libraries에 BGW 모듈 등록
RUN echo "shared_preload_libraries = 'postfga'" >> /usr/share/postgresql/postgresql.conf.sample

# 포트/볼륨은 postgres 기본값 그대로 사용