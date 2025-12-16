# syntax=docker/dockerfile:1.7

# ------------------------------------------------------------
# Global argument for PostgreSQL major version
# ------------------------------------------------------------
ARG PG_MAJOR=18

# ------------------------------------------------------------
# Stage 0: Base build environment (build-base)
# - Installs build toolchain and PostgreSQL server headers
# ------------------------------------------------------------
FROM postgres:${PG_MAJOR} AS build-base

ARG PG_MAJOR

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        gdb \
        cmake \
        git \
        autoconf \
        libtool \
        pkg-config \
        bear \
        libgrpc++-dev \
        libprotobuf-dev \
        protobuf-compiler-grpc \
        libxxhash-dev \
        postgresql-server-dev-${PG_MAJOR}

WORKDIR /workspace

# ------------------------------------------------------------
# Stage 1: Development environment (dev)
# - Used by VSCode DevContainer
# - Includes Node.js, npm, and a non-root user
# ------------------------------------------------------------
FROM build-base AS dev

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        nodejs \
        npm

# Create VSCode-friendly non-root user
RUN useradd -ms /bin/bash vscode

# ------------------------------------------------------------
# Stage 2: Release build environment (release-builder)
# ------------------------------------------------------------
FROM build-base AS release-builder

# Copy full project (use .dockerignore to exclude unnecessary files)
COPY . .

# Configure and build
RUN cmake --preset release && \
    cmake --build --preset=release

# Install to temporary location
RUN DESTDIR=/out cmake --install build/release

# ------------------------------------------------------------
# Stage 3: Runtime image
# ------------------------------------------------------------
FROM postgis/postgis:${PG_MAJOR}-3.6 AS runtime

ARG PG_MAJOR

# Install runtime dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        libgrpc++ \
        libprotobuf32 \
    && rm -rf /var/lib/apt/lists/*


COPY --from=release-builder --chmod=644 /workspace/initdb-postfga.sh /docker-entrypoint-initdb.d/10_postfga.sh

# Copy extension shared libraries
COPY --from=release-builder /out/usr/lib/postgresql/${PG_MAJOR}/lib/postfga.so \
    /usr/lib/postgresql/${PG_MAJOR}/lib/

# Copy extension control / SQL files
COPY --from=release-builder /out/usr/share/postgresql/${PG_MAJOR}/extension/* \
    /usr/share/postgresql/${PG_MAJOR}/extension/
