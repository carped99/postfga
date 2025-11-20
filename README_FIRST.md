# 🎯 OpenFGA FDW PostgreSQL Extension - Start Here

**Welcome!** This document is your entry point to the OpenFGA FDW PostgreSQL extension project.

## ✨ What Is This Project?

A **PostgreSQL 18+ Foreign Data Wrapper (FDW) extension** that integrates **OpenFGA authorization checks** directly into SQL queries. Query OpenFGA permissions without leaving PostgreSQL.

```sql
-- Query OpenFGA permissions directly in PostgreSQL
SELECT 1 FROM acl_permission
 WHERE object_type  = 'document'
   AND object_id    = '123'
   AND subject_type = 'user'
   AND subject_id   = 'alice'
   AND relation     = 'read'
LIMIT 1;  -- 1 row = allowed, 0 rows = denied
```

## 🚀 Quick Start (5 minutes)

```bash
# 1. Open in VSCode with devcontainer
code .
# → VSCode will prompt to "Reopen in Container" - click it

# 2. Once in container, generate protobuf code
make proto-generate

# 3. Build the extension
make clean
make
make install

# 4. Create the extension in PostgreSQL
psql -U postgres -c "CREATE EXTENSION openfga_fdw;"

# 5. Create a server connection to OpenFGA
psql -U postgres -c "
  CREATE SERVER openfga_server FOREIGN DATA WRAPPER openfga_fdw
  OPTIONS (
    endpoint 'dns:///openfga:8081',
    store_id 'your-store-id'
  );
"

# 6. Done! Now use acl_permission table to query permissions
```

## 📚 Documentation Guide

### Start With These (In Order)

1. **[README.md](README.md)** - Main documentation (~400 lines)
   - Overview, installation, configuration
   - Usage examples and troubleshooting
   - **Read this first**

2. **[GETTING_STARTED.md](GETTING_STARTED.md)** - Practical guide (~350 lines)
   - Step-by-step setup
   - Real-world examples
   - Common scenarios

3. **[ARCHITECTURE.md](ARCHITECTURE.md)** - Technical deep dive (~600 lines)
   - Component design
   - Data flow diagrams
   - Performance characteristics

### For Development

4. **[DEVELOPMENT.md](DEVELOPMENT.md)** - Development guide (~400 lines)
   - Setting up dev environment
   - Code standards
   - Debugging techniques

5. **[proto/README.md](proto/README.md)** - Protocol Buffers (~220 lines)
   - Proto folder structure
   - buf code generation
   - Build flow

6. **[proto/BUF_DOCKER_GUIDE.md](proto/BUF_DOCKER_GUIDE.md)** - Docker guide (~350 lines)
   - Standalone buf operations
   - Multiple usage methods
   - Troubleshooting

### Reference

7. **[PROJECT_COMPLETE_INDEX.md](PROJECT_COMPLETE_INDEX.md)** - Master index
   - Complete file structure
   - All components documented
   - Statistics and status

8. **[FINAL_SUMMARY.md](FINAL_SUMMARY.md)** - Project summary
   - High-level overview
   - Implementation status
   - Next steps

## 🏗️ Project Structure

```
openfga_fdw/
├── Documentation (13 markdown files, 3000+ lines)
│   ├── README.md ........................... Main documentation
│   ├── GETTING_STARTED.md .................. Quick start guide
│   ├── ARCHITECTURE.md ..................... Technical design
│   ├── DEVELOPMENT.md ...................... Dev guide
│   └── More... (see README.md)
│
├── Source Code (16 C/C++ files, 2000+ lines)
│   ├── include/ ............................ 5 header files
│   ├── src/ ................................ 8 C files
│   ├── src/grpc/ ........................... 1 C++ file + generated code
│   └── sql/ ................................ 2 SQL files
│
├── Protocol Buffers (9 files)
│   ├── proto/Dockerfile .................... Standalone buf container
│   ├── proto/buf.yaml ...................... Workspace config
│   ├── proto/buf.gen.yaml .................. Code generation
│   ├── proto/openfga_fdw.proto ............. Service definitions
│   └── More... (see proto/README.md)
│
├── Docker (4 files)
│   ├── Dockerfile.dev ...................... Development image
│   ├── Dockerfile.prod ..................... Production image
│   ├── docker-compose.dev.yml .............. Orchestration
│   └── initdb.d/ ........................... Initialization scripts
│
├── Development (5 files)
│   ├── .vscode/settings.json ............... Editor config
│   ├── .vscode/c_cpp_properties.json ....... C++ config
│   ├── .vscode/launch.json ................. Debug configs (3 profiles)
│   ├── .vscode/tasks.json .................. Build tasks (8 tasks)
│   └── .devcontainer/devcontainer.json .... Remote container
│
├── Build & Config (3 files)
│   ├── Makefile ............................ Build system (15+ targets)
│   ├── .gitignore .......................... Git patterns
│   └── .dockerignore ....................... Docker patterns
│
├── Scripts (2 files)
│   ├── scripts/buf-docker.sh ............... buf Docker wrapper
│   └── proto/entrypoint.sh ................. buf container entrypoint
│
└── Project Info (1 file)
    └── CLAUDE.md ........................... Project specifications
```

## 🎯 Key Features

- ✅ **Read-only FDW**: Query OpenFGA permissions in SQL
- ✅ **Smart Caching**: Generation-based invalidation with LWLock
- ✅ **Background Worker**: Async change stream synchronization
- ✅ **gRPC Integration**: Direct OpenFGA API communication
- ✅ **Protocol Buffers**: Automated code generation with buf
- ✅ **Docker Ready**: Dev and production containers included
- ✅ **VSCode Support**: Devcontainer with full IDE setup
- ✅ **Comprehensive Docs**: 3000+ lines of documentation

## 🔧 Build & Development

### Option 1: VSCode DevContainer (Recommended)
```bash
# Open in VSCode
code .

# Click "Reopen in Container" when prompted
# Everything is automatically set up

# Build
make proto-generate
make clean
make
make install
```

### Option 2: Manual Docker
```bash
docker build -t openfga-fdw-dev:latest docker/
docker run -it -v $(pwd):/workspace openfga-fdw-dev:latest bash
cd /workspace
make proto-generate
make clean
make
make install
```

### Option 3: Local Setup (Linux/macOS)
```bash
# Install PostgreSQL dev headers
sudo apt-get install postgresql-server-dev-18  # Ubuntu/Debian
# or
brew install postgresql@18                      # macOS

# Install gRPC and Protobuf
sudo apt-get install libgrpc++-dev libprotobuf-dev  # Ubuntu/Debian
# or
brew install grpc protobuf                      # macOS

# Install buf
curl -sSL https://github.com/bufbuild/buf/releases/download/v1.28.0/buf-Linux-x86_64 -o buf
chmod +x buf
sudo mv buf /usr/local/bin/

# Build
make proto-generate
make clean
make
make install
```

## 📦 Installation

### From Source
```bash
make install
psql -U postgres -c "CREATE EXTENSION openfga_fdw;"
```

### From Docker
```bash
docker build -f docker/Dockerfile.prod -t openfga-fdw:latest .
docker run -d --name postgres-openfga -e POSTGRES_PASSWORD=secret openfga-fdw:latest
```

## 🔌 Usage Example

### Step 1: Create Server
```sql
CREATE SERVER openfga_server FOREIGN DATA WRAPPER openfga_fdw
OPTIONS (
  endpoint 'dns:///openfga:8081',
  store_id 'your-store-id'
);
```

### Step 2: Create Foreign Table
```sql
CREATE FOREIGN TABLE acl_permission (
  object_type   text,
  object_id     text,
  subject_type  text,
  subject_id    text,
  relation      text
) SERVER openfga_server;
```

### Step 3: Query Permissions
```sql
-- Check if alice can read document 123
SELECT COUNT(*) FROM acl_permission
 WHERE object_type  = 'document'
   AND object_id    = '123'
   AND subject_type = 'user'
   AND subject_id   = 'alice'
   AND relation     = 'read';

-- 1 = allowed, 0 = denied
```

## 🐛 Troubleshooting

### Build Issues
See [DEVELOPMENT.md](DEVELOPMENT.md#troubleshooting)

### Runtime Issues
See [README.md](README.md#troubleshooting)

### Docker Issues
See [proto/BUF_DOCKER_GUIDE.md](proto/BUF_DOCKER_GUIDE.md#troubleshooting)

## 📋 Makefile Targets

```bash
make                    # Build extension
make install            # Install to PostgreSQL
make clean              # Clean build artifacts
make clean-all          # Clean including generated proto
make proto-check        # Check buf availability
make proto-generate     # Generate C++ gRPC code
make proto-lint         # Lint proto files
make proto-format       # Format proto files
make proto-breaking     # Check for breaking changes
make proto-update       # Update dependencies
make install-dev        # Install with dev instructions
make test               # Run tests
make help               # Show all targets
```

## 🎓 Learning Path

1. **Understand the project**
   - Read [README.md](README.md)
   - Skim [ARCHITECTURE.md](ARCHITECTURE.md)

2. **Set up environment**
   - Open in VSCode devcontainer
   - Or follow [DEVELOPMENT.md](DEVELOPMENT.md)

3. **Build and test**
   - Run `make proto-generate`
   - Run `make clean && make`
   - Test with sample queries

4. **Dive deeper**
   - Review [ARCHITECTURE.md](ARCHITECTURE.md) for design
   - Check [src/](src/) for implementation
   - Explore [proto/](proto/) for Protocol Buffers

5. **Contribute**
   - Follow [DEVELOPMENT.md](DEVELOPMENT.md) guidelines
   - Add tests for changes
   - Update documentation

## 🤝 Contributing

1. Read [DEVELOPMENT.md](DEVELOPMENT.md)
2. Set up devcontainer
3. Create feature branch
4. Make changes
5. Test thoroughly
6. Submit pull request

## 📞 Getting Help

| Issue | Resource |
|-------|----------|
| Installation | [README.md](README.md#installation) |
| Configuration | [GETTING_STARTED.md](GETTING_STARTED.md) |
| Development | [DEVELOPMENT.md](DEVELOPMENT.md) |
| Architecture | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Proto/buf | [proto/BUF_DOCKER_GUIDE.md](proto/BUF_DOCKER_GUIDE.md) |
| Troubleshooting | [README.md](README.md#troubleshooting) |
| File Reference | [PROJECT_COMPLETE_INDEX.md](PROJECT_COMPLETE_INDEX.md) |

## 📊 Project Statistics

| Metric | Value |
|--------|-------|
| **Documentation** | 13 files, 3000+ lines |
| **Source Code** | 16 files, 2000+ lines (C/C++) |
| **Proto Files** | 9 files |
| **Docker Files** | 4 files |
| **Config Files** | 8 files |
| **Total Files** | 50+ files |
| **Makefile Targets** | 15+ targets |
| **Build Tasks** | 8 tasks |
| **Debug Profiles** | 3 profiles |
| **GUC Parameters** | 8 parameters |

## ✅ Completion Status

- ✅ Core extension implementation
- ✅ Cache system with generation tracking
- ✅ Background worker for change processing
- ✅ gRPC integration
- ✅ Protocol Buffers setup
- ✅ Build system (PGXS + proto)
- ✅ Docker support (dev + prod)
- ✅ VSCode devcontainer
- ✅ Comprehensive documentation
- ⏳ Test suite (ready for contribution)
- ⏳ CI/CD pipeline (ready for integration)

## 🎉 What's Next?

1. **Try it out**: Follow Quick Start above
2. **Explore code**: Check out [src/](src/) directory
3. **Learn design**: Read [ARCHITECTURE.md](ARCHITECTURE.md)
4. **Contribute tests**: Add to test suite
5. **Optimize**: Profile and improve performance

## 📄 License

[Your License Here]

## 👤 Author

[Your Name]

---

## 🗺️ Document Map

```
README_FIRST.md (you are here)
    ↓
README.md (main docs)
    ├─→ GETTING_STARTED.md (quick start)
    ├─→ ARCHITECTURE.md (technical)
    ├─→ DEVELOPMENT.md (dev setup)
    │
    └─→ proto/README.md (proto folder)
        └─→ proto/BUF_DOCKER_GUIDE.md (docker)

Other documents:
    ├─→ PROJECT_COMPLETE_INDEX.md (master index)
    ├─→ FINAL_SUMMARY.md (project summary)
    ├─→ IMPLEMENTATION_CHECKLIST.md (status)
    └─→ TASK_COMPLETION_SUMMARY.md (recent work)
```

---

**Ready to start?** → Open [README.md](README.md)

**Questions about structure?** → See [PROJECT_COMPLETE_INDEX.md](PROJECT_COMPLETE_INDEX.md)

**Want to develop?** → Follow [DEVELOPMENT.md](DEVELOPMENT.md)

---

**Project Status**: ✅ Implementation Complete, Ready for Testing

**Last Updated**: 2025-01-18

**Version**: 1.0.0-beta
