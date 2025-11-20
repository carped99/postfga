# Build Instructions

## Prerequisites

- PostgreSQL 18+ with development headers
- C++20 compatible compiler (GCC 10+, Clang 13+)
- CMake 3.20+ (for CMake build)
- gRPC and Protobuf libraries
- Make (for PGXS build)

## Build Methods

### Method 1: CMake (Recommended)

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build
cmake --build .

# Install
sudo cmake --install .

# Show configuration
cmake --build . --target show-config

# Uninstall
sudo cmake --build . --target uninstall
```

### Method 2: PGXS (PostgreSQL Extension Build System)

```bash
# Build
make

# Install
sudo make install

# Clean
make clean

# Clean all (including generated proto)
make clean-all
```

## PostgreSQL Configuration

After installation, add to `postgresql.conf`:

```ini
shared_preload_libraries = 'openfga_fdw'

# PostFGA settings
postfga.endpoint = 'localhost:8081'
postfga.store_id = 'your-store-id'
postfga.bgw_workers = 1
postfga.cache_ttl_ms = 60000
```

Restart PostgreSQL:

```bash
sudo systemctl restart postgresql
```

## Create Extension

```sql
CREATE EXTENSION openfga_fdw;

CREATE SERVER openfga_server
    FOREIGN DATA WRAPPER openfga_fdw
    OPTIONS (
        endpoint 'localhost:8081',
        store_id 'your-store-id'
    );

CREATE FOREIGN TABLE acl_permission (
    object_type text,
    object_id text,
    subject_type text,
    subject_id text,
    relation text
) SERVER openfga_server;
```

## Development Build

```bash
# With CMake
mkdir build-dev
cd build-dev
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .

# With Make
make install-dev
```

## Troubleshooting

### pg_config not found

```bash
# Find PostgreSQL installation
which pg_config

# Or install PostgreSQL dev package
# Ubuntu/Debian
sudo apt-get install postgresql-server-dev-18

# RHEL/CentOS
sudo yum install postgresql18-devel
```

### gRPC not found

```bash
# Ubuntu/Debian
sudo apt-get install libgrpc++-dev libprotobuf-dev

# Or build from source
git clone https://github.com/grpc/grpc
cd grpc
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### Permission denied during install

Use `sudo` for installation:

```bash
sudo cmake --install .
# or
sudo make install
```
