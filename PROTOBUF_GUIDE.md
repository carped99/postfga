# Protocol Buffers & gRPC Setup Guide

This guide explains how to generate C++ gRPC code from Protocol Buffer definitions for the OpenFGA FDW extension.

## Overview

The project uses:
- **Buf** - Modern Protocol Buffer tooling (v2 spec)
- **OpenFGA API** - Official OpenFGA service definitions from `buf.build/openfga/api`
- **gRPC C++** - C++ bindings for gRPC services
- **Protocol Buffers** - Data serialization format

## Directory Structure

```
proto/
├── buf.yaml                    # Buf configuration (workspace)
├── buf.gen.yaml               # Code generation configuration
├── buf.lock                   # Dependency lock file (auto-generated)
└── openfga_fdw.proto         # FDW-specific service definitions

src/grpc/
├── generated/                # Generated code (auto-generated)
│   ├── openfga_fdw.pb.cc     # Protocol Buffer C++ impl
│   ├── openfga_fdw.pb.h      # Protocol Buffer C++ header
│   ├── openfga_fdw.grpc.pb.cc # gRPC service impl
│   └── openfga_fdw.grpc.pb.h # gRPC service header
└── client.cpp                # gRPC client implementation
```

## Prerequisites

### Install Buf

**macOS**:
```bash
brew install bufbuild/buf/buf
```

**Linux (Ubuntu/Debian)**:
```bash
curl -sSL https://github.com/bufbuild/buf/releases/download/v1.28.0/buf-Linux-x86_64 -o /usr/local/bin/buf
chmod +x /usr/local/bin/buf
```

**Windows (WSL2)**:
```bash
# Use Linux instructions above inside WSL2
```

**Docker**:
```bash
docker run --rm -v $(pwd):/workspace bufbuild/buf:latest generate
```

### Verify Installation

```bash
buf --version
# Output: buf version 1.28.0

buf beta version
# Output: (proto)
```

## Generating Code

### Quick Start

```bash
# Generate all C++ gRPC code
make proto-generate

# Output:
# Generating gRPC stub code from OpenFGA API...
# gRPC code generation complete
# Generated files:
# src/grpc/generated/openfga_fdw.pb.cc
# src/grpc/generated/openfga_fdw.pb.h
# src/grpc/generated/openfga_fdw.grpc.pb.cc
# src/grpc/generated/openfga_fdw.grpc.pb.h
```

### Manual Generation

```bash
cd proto

# Generate C++ code
buf generate

# With verbose output
buf generate --verbose

# Generate only specific language
buf generate --template buf.gen.yaml
```

## Buf Configuration

### buf.yaml (Workspace Definition)

```yaml
version: v1
build:
  roots:
    - .
deps:
  - buf.build/openfga/api    # OpenFGA API dependency
lint:
  use:
    - STANDARD                # Standard lint rules
breaking:
  use:
    - STANDARD                # Standard breaking change detection
```

**Key Features**:
- `version: v1` - Buf v1+ format
- `deps` - Import OpenFGA's published protobuf definitions
- `lint` - Code style enforcement
- `breaking` - Detect incompatible changes

### buf.gen.yaml (Code Generation)

```yaml
version: v1
managed:
  enabled: true
  go_package_prefix:
    default: github.com/tykim/openfga_fdw/gen/go
  java_package_prefix:
    default: com.tykim.openfga_fdw.gen.java
  java_multiple_files: true
plugins:
  # C++ gRPC service stubs
  - remote: buf.build/grpc/cpp
    out: ../src/grpc/generated
    opt:
      - paths=source_relative

  # C++ Protocol Buffers
  - remote: buf.build/protocolbuffers/cpp
    out: ../src/grpc/generated
    opt:
      - paths=source_relative

  # Go (for testing/tooling)
  - remote: buf.build/grpc/go
    out: ../gen/go
    opt:
      - paths=source_relative

  # Go Protocol Buffers
  - remote: buf.build/protocolbuffers/go
    out: ../gen/go
    opt:
      - paths=source_relative
```

**Key Settings**:
- `managed.enabled: true` - Use managed mode for consistency
- `plugins` - Define code generators:
  - `grpc/cpp` - C++ gRPC service stubs
  - `protocolbuffers/cpp` - C++ Protocol Buffers
  - `grpc/go` - Go gRPC (for testing)
- `paths=source_relative` - Generate in proto file directory

## Proto File Structure

### openfga_fdw.proto

**Import OpenFGA Types**:
```protobuf
syntax = "proto3";
package openfga.fdw;

import "openfga/v1/openfga.proto";

option go_package = "github.com/tykim/openfga_fdw/gen/go/openfga/fdw";
```

**Define Service**:
```protobuf
service OpenFGAFdwService {
  rpc Check(CheckRequest) returns (CheckResponse);
  rpc ReadChanges(ReadChangesRequest) returns (stream ReadChangesResponse);
  rpc BatchCheck(BatchCheckRequest) returns (BatchCheckResponse);
}
```

**Message Definitions**:
```protobuf
message CheckRequest {
  openfga.v1.CheckRequest request = 1;
  CachePolicy cache_policy = 2;
  int32 timeout_ms = 3;
}

message CheckResponse {
  bool allowed = 1;
  openfga.v1.CheckResponse openfga_response = 2;
  CacheInfo cache_info = 3;
}
```

## Makefile Targets

### Code Generation

```bash
# Generate C++ gRPC code from proto files
make proto-generate

# Update proto dependencies from buf.build
make proto-update
```

### Validation

```bash
# Lint proto files (style, naming, structure)
make proto-lint

# Check for breaking changes
make proto-breaking
```

### Formatting

```bash
# Auto-format proto files
make proto-format
```

### Cleanup

```bash
# Remove generated files
make clean-all
```

## Using Generated Code

### In C++ Implementation

**Include Generated Headers**:
```cpp
#include "openfga_fdw.pb.h"
#include "openfga_fdw.grpc.pb.h"

using openfga::fdw::CheckRequest;
using openfga::fdw::CheckResponse;
using openfga::fdw::OpenFGAFdwService;
```

**Create Request**:
```cpp
CheckRequest request;
request.mutable_request()->set_store_id("my-store");
request.mutable_request()->mutable_object()->set_type("document");
request.mutable_request()->mutable_object()->set_id("doc-123");
request.mutable_request()->mutable_relation()->assign("read");
request.mutable_request()->mutable_subject()->set_type("user");
request.mutable_request()->mutable_subject()->set_id("alice");
```

**Handle Response**:
```cpp
CheckResponse response = client.Check(request);
if (response.allowed()) {
    // Permission granted
    elog(DEBUG2, "Permission check passed");
} else {
    // Permission denied
    elog(DEBUG2, "Permission check failed");
}
```

### gRPC Client Usage

**Create Channel**:
```cpp
auto channel = grpc::CreateChannel("localhost:8081",
                                  grpc::InsecureChannelCredentials());
```

**Create Stub**:
```cpp
auto stub = OpenFGAFdwService::NewStub(channel);
```

**Call Service**:
```cpp
CheckRequest request;
CheckResponse response;
grpc::ClientContext context;

grpc::Status status = stub->Check(&context, request, &response);

if (status.ok()) {
    // Handle response
    std::cout << "Allowed: " << response.allowed() << std::endl;
} else {
    std::cerr << "Error: " << status.error_message() << std::endl;
}
```

## Troubleshooting

### "buf: command not found"

**Solution**: Install Buf
```bash
brew install bufbuild/buf/buf
# Or see Prerequisites section
```

### Generated Files Not Created

**Check Buf Configuration**:
```bash
cd proto && buf ls-files
# Should show proto files
```

**Verify buf.gen.yaml**:
```bash
cd proto && buf generate --verbose
# Shows detailed generation process
```

**Check Permissions**:
```bash
ls -la src/grpc/
# Should be writable
```

### Import Error: "openfga/v1/openfga.proto"

**Solution**: Update proto dependencies
```bash
cd proto && buf mod update
# Downloads OpenFGA API definitions
```

### Breaking Changes Detected

When updating OpenFGA API:
```bash
cd proto && buf breaking --against HEAD
# Shows what changed

# If breaking changes are acceptable
buf breaking --against HEAD --only-imports
```

## Buf Lint Rules

Proto files are linted against `STANDARD` rules:

**Style Rules**:
- File names use snake_case
- Package names use lowercase.separated.form
- Message names use PascalCase
- Field names use snake_case

**Structure Rules**:
- Imports organized
- Comments for public APIs
- Reserved keywords handled
- Proper syntax

**Run Linting**:
```bash
make proto-lint
```

**Fix Issues**:
```bash
make proto-format    # Auto-format to fix style
```

## OpenFGA API Usage

The project imports OpenFGA's official API:

```protobuf
import "openfga/v1/openfga.proto";
```

**Available Types**:
- `openfga.v1.CheckRequest` - Permission check
- `openfga.v1.CheckResponse` - Check result
- `openfga.v1.ReadChangesRequest` - Stream changes
- `openfga.v1.WriteRequest` - Update tuples
- `openfga.v1.Tuple` - Relationship tuple
- `openfga.v1.TupleChange` - Tuple change event

**Source**: https://buf.build/openfga/api

## CI/CD Integration

### GitHub Actions

```yaml
name: Proto Generate and Lint

on: [push, pull_request]

jobs:
  proto:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - uses: bufbuild/buf-setup-action@v1

      - run: buf lint proto/

      - run: cd proto && buf generate

      - run: git diff --exit-code
        # Fail if generation changes git state
```

### Pre-commit Hook

```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/bufbuild/buf
    rev: v1.28.0
    hooks:
      - id: buf-lint
        args: ['lint', 'proto/']

      - id: buf-format
        args: ['format', '-w', 'proto/']

      - id: buf-generate
```

## Performance Notes

**Generation Time**:
- Initial generation: ~2-5 seconds (downloading deps)
- Incremental: <1 second
- Docker: ~5-10 seconds (includes container overhead)

**Generated Code Size**:
- openfga_fdw.pb.* : ~50-100 KB
- openfga_fdw.grpc.pb.* : ~30-50 KB
- Total: ~100-150 KB for generated code

**Dependencies**:
- OpenFGA API proto files: ~500 KB (downloaded once)
- Cached in `proto/buf.lock`

## Next Steps

1. **Generate Initial Code**
   ```bash
   make proto-generate
   ```

2. **Review Generated Files**
   ```bash
   ls -la src/grpc/generated/
   ```

3. **Implement gRPC Client**
   ```cpp
   // src/grpc/client.cpp
   ```

4. **Update Build System**
   - Add generated objects to Makefile OBJS
   - Link gRPC libraries

5. **Integration Testing**
   - Test against local OpenFGA instance
   - Verify serialization

## Resources

- [Buf Documentation](https://buf.build/docs)
- [OpenFGA API Reference](https://buf.build/openfga/api)
- [gRPC C++ Guide](https://grpc.io/docs/languages/cpp/)
- [Protocol Buffers Guide](https://developers.google.com/protocol-buffers)

## Version Information

- **Buf Version**: v1.28.0+
- **OpenFGA API**: Latest from buf.build/openfga/api
- **gRPC**: Compatible with gRPC 1.50+
- **Protocol Buffers**: Proto3 syntax

---

For more information, see:
- [DEVELOPMENT.md](DEVELOPMENT.md) - Development workflow
- [Makefile](Makefile) - Build targets
- [proto/buf.yaml](proto/buf.yaml) - Buf workspace config
