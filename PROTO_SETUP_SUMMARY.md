# Protocol Buffers & buf Docker Setup - Complete

## Overview

A complete, **standalone, independently operational** buf Docker setup has been created for Protocol Buffer code generation and validation. The setup requires only Docker - no local buf CLI, protoc, or gRPC tools needed.

## What Was Created

### Proto Directory Structure (`proto/`)

```
proto/
├── Dockerfile               # Standalone buf Docker container
├── entrypoint.sh           # Intelligent command router (~90 lines)
├── docker-compose.yml      # Simplified compose-based workflow
├── buf.yaml                # Buf workspace configuration (v1)
├── buf.gen.yaml            # Code generation config (plugins → src/grpc/generated/)
├── buf.lock                # Dependency version lock (auto-generated)
├── openfga_fdw.proto       # Service definitions (~200 lines)
├── README.md               # Quick start and overview (~220 lines)
├── BUF_DOCKER_GUIDE.md     # Comprehensive Docker guide (~350 lines)
└── SETUP_COMPLETE.md       # Setup completion details

.gitignore includes:
  - proto/buf.lock (generated)
  - src/grpc/generated/ (generated code)
```

## Key Features

### 1. Standalone Docker Setup
- **No local tool requirements**: Works with Docker only
- **Self-contained Dockerfile**: Based on `bufbuild/buf:latest`
- **Intelligent entrypoint.sh**: Validates setup, provides helpful errors
- **Docker Compose integration**: Simplified workflow with docker-compose

### 2. Multiple Usage Methods

#### Method 1: Direct Docker Commands
```bash
docker build -t openfga-fdw-buf:latest proto/
docker run --rm -v $(pwd):/workspace openfga-fdw-buf:latest generate
docker run --rm -v $(pwd):/workspace openfga-fdw-buf:latest lint proto/
docker run --rm -v $(pwd):/workspace openfga-fdw-buf:latest format -w proto/
```

#### Method 2: Docker Compose (Recommended)
```bash
cd proto/
docker-compose build
docker-compose run --rm buf generate
docker-compose run --rm buf lint proto/
docker-compose run --rm buf format -w proto/
```

#### Method 3: Makefile Integration
```bash
make proto-check        # Verify buf is available
make proto-generate     # Generate C++ code
make proto-lint         # Validate proto files
make proto-format       # Auto-format proto files
make proto-breaking     # Check for breaking changes
make proto-update       # Update dependencies
```

### 3. Comprehensive Documentation

#### proto/README.md
- Proto directory overview
- Quick start for all 3 usage methods
- Configuration file explanations
- Build flow diagram
- Troubleshooting guide
- Development workflow

#### proto/BUF_DOCKER_GUIDE.md
- Detailed Docker setup guide
- Command reference with examples
- Volume mounting explanation
- Permissions and ownership troubleshooting
- CI/CD integration (GitHub Actions example)
- Performance considerations
- 10+ troubleshooting scenarios

## Integration with Project

### 1. Makefile Integration
The root `Makefile` automatically detects and uses this setup:

```makefile
# Checks for local buf CLI
HAS_BUF := $(shell command -v buf >/dev/null 2>&1 && echo 1 || echo 0)
HAS_DOCKER := $(shell command -v docker >/dev/null 2>&1 && echo 1 || echo 0)

# Intelligently selects:
# - Local buf if available
# - Docker wrapper script if Docker available
# - Error if neither available
```

All proto targets automatically use Docker when local buf unavailable:
```bash
make proto-generate     # Works with or without local buf
make proto-lint
make proto-format
make proto-check        # Shows which method is being used
```

### 2. Makefile Proto Targets

```makefile
proto-check       # Verify buf availability (local or Docker)
proto-generate    # Generate C++ gRPC stubs and protobuf messages
proto-lint        # Validate proto files for style compliance
proto-format      # Auto-format proto files
proto-breaking    # Check for breaking changes
proto-update      # Update dependencies from buf registry
```

### 3. Generated Code Output

When you run `make proto-generate` or `docker-compose run --rm buf generate`:

```
src/grpc/generated/
├── openfga_fdw.pb.h        # Protobuf message declarations
├── openfga_fdw.pb.cc       # Protobuf message implementations
├── openfga_fdw.grpc.pb.h   # gRPC service stubs (declarations)
└── openfga_fdw.grpc.pb.cc  # gRPC service stubs (implementations)
```

These are automatically compiled into the PostgreSQL extension binary (`openfga_fdw.so`).

## Configuration Details

### buf.yaml (Workspace Configuration)
```yaml
version: v1
build:
  roots:
    - .
deps:
  - buf.build/openfga/api
```

- **v1 specification**: Modern buf configuration
- **Single root**: All proto files in proto/ directory
- **OpenFGA dependency**: Imports types from buf.build/openfga/api

### buf.gen.yaml (Code Generation)
```yaml
version: v1
plugins:
  - remote: buf.build/grpc/cpp
    out: ../src/grpc/generated
  - remote: buf.build/protocolbuffers/cpp
    out: ../src/grpc/generated
```

- **C++ gRPC plugin**: Generates service stubs
- **C++ Protobuf plugin**: Generates message types
- **Output location**: `src/grpc/generated/` (relative to proto/)

### openfga_fdw.proto (Service Definitions)
Contains:
- `OpenFGAFdwService` with RPC methods:
  - `Check`: Single permission check
  - `ReadChanges`: Stream of authorization changes
  - `BatchCheck`: Multiple checks in one request
- Message types with proper documentation
- gRPC service definition

## Usage Scenarios

### Scenario 1: Local Development Without buf CLI

Developer has Docker, but no local buf CLI:

```bash
# First time setup
cd openfga_fdw
docker build -t openfga-fdw-buf:latest proto/

# Generate code
docker run --rm -v $(pwd):/workspace openfga-fdw-buf:latest generate

# Or use make (automatically uses Docker)
make proto-generate

# Generated files in src/grpc/generated/
```

### Scenario 2: Using Docker Compose

Prefer compose-based workflow:

```bash
cd proto/
docker-compose build                    # Build once
docker-compose run --rm buf generate
docker-compose run --rm buf lint proto/
docker-compose run --rm buf format -w proto/
```

### Scenario 3: CI/CD Integration

GitHub Actions example (see `proto/BUF_DOCKER_GUIDE.md`):

```yaml
- name: Generate protobuf code
  run: docker run --rm -v ${{ github.workspace }}:/workspace openfga-fdw-buf:latest generate

- name: Lint proto files
  run: docker run --rm -v ${{ github.workspace }}:/workspace openfga-fdw-buf:latest lint proto/
```

### Scenario 4: Team Development

Mixed tools in team:

- Developer A: Has local buf → `make proto-generate` uses local
- Developer B: Has Docker only → `make proto-generate` automatically uses Docker
- CI/CD: Uses Docker → `make proto-generate` works in pipeline
- **All get same results** thanks to intelligent Makefile detection

## File Ownership (Linux/WSL)

Docker on Linux generates files owned by root. Solutions:

### Option 1: Use docker-compose with UID/GID
```bash
cd proto/
UID=$(id -u) GID=$(id -g) docker-compose run --rm buf generate
cd ..
```

### Option 2: Fix permissions after
```bash
sudo chown -R $USER src/grpc/generated/
```

### Option 3: Use Docker Desktop (macOS/Windows)
Docker Desktop handles permissions automatically.

## Troubleshooting

**Problem**: "buf command not found"
```bash
# Check for local buf
buf --version

# If not found, make sure Docker is available
docker --version

# Makefile will automatically use Docker
make proto-check    # Shows which method is active
```

**Problem**: Generated files not found
```bash
# Ensure volume mount is correct
docker run --rm -v $(pwd):/workspace openfga-fdw-buf:latest --help

# Check if generation succeeded
ls -la src/grpc/generated/

# Regenerate if needed
rm -rf src/grpc/generated/
make proto-generate
```

**Problem**: "buf.yaml not found" warning
```bash
# Ensure project root is mounted, not just proto/
# Correct: docker run -v $(pwd):/workspace ...
# Wrong: docker run -v ./proto:/workspace ...
```

**Problem**: Dependency fetch errors
```bash
# Update dependencies
make proto-update

# Or manually
docker run --rm -v $(pwd):/workspace openfga-fdw-buf:latest mod update proto/
```

## Next Steps

1. **Verify Docker is installed**:
   ```bash
   docker --version
   ```

2. **Build the buf Docker image**:
   ```bash
   docker build -t openfga-fdw-buf:latest proto/
   ```

3. **Generate code**:
   ```bash
   make proto-generate
   ```

4. **Verify generation succeeded**:
   ```bash
   ls -la src/grpc/generated/
   # Should show: openfga_fdw.pb.h, openfga_fdw.pb.cc, openfga_fdw.grpc.pb.h, openfga_fdw.grpc.pb.cc
   ```

5. **Check Makefile integration**:
   ```bash
   make proto-check    # Shows which buf method is available
   ```

6. **Compile the extension**:
   ```bash
   make clean
   make
   ```

## Documentation Files

- **proto/README.md**: Quick start and overview for proto folder
- **proto/BUF_DOCKER_GUIDE.md**: Comprehensive Docker setup guide (350+ lines)
- **proto/SETUP_COMPLETE.md**: Setup completion and verification details
- **PROTO_SETUP_SUMMARY.md** (this file): Project-level summary

## Project Integration Status

| Component | Status | Details |
|-----------|--------|---------|
| Dockerfile | ✓ Complete | Standalone buf container in proto/ |
| entrypoint.sh | ✓ Complete | Intelligent command routing |
| docker-compose.yml | ✓ Complete | Simplified workflow |
| buf.yaml | ✓ Complete | Workspace configuration |
| buf.gen.yaml | ✓ Complete | C++ code generation |
| openfga_fdw.proto | ✓ Complete | Service definitions |
| Makefile integration | ✓ Complete | Auto-detects and uses Docker |
| Documentation | ✓ Complete | Comprehensive guides |
| CI/CD ready | ✓ Complete | GitHub Actions example included |

## Key Achievements

✓ **Completely standalone**: Works with Docker only, no local tool requirements
✓ **Intelligent fallback**: Makefile detects local buf, falls back to Docker
✓ **Docker Compose support**: Simplified workflow with docker-compose.yml
✓ **Comprehensive docs**: 350+ lines of guides and examples
✓ **CI/CD ready**: Examples for GitHub Actions included
✓ **Proper permissions**: UID/GID support in docker-compose
✓ **Error handling**: Intelligent entrypoint with helpful messages
✓ **Development friendly**: Works seamlessly with existing workflow

## References

- **buf Documentation**: https://buf.build/docs
- **buf CLI Reference**: https://buf.build/docs/cli-overview
- **OpenFGA API (buf.build)**: https://buf.build/openfga/api
- **Protocol Buffers**: https://developers.google.com/protocol-buffers
- **gRPC C++**: https://grpc.io/docs/languages/cpp/

For detailed guides, see:
- `proto/README.md` - Proto folder quick start
- `proto/BUF_DOCKER_GUIDE.md` - Comprehensive Docker guide
- `proto/SETUP_COMPLETE.md` - Setup verification
