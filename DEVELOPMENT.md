# Development Guide

This guide covers development setup, testing, and contribution guidelines for openfga_fdw.

## Development Environment Setup

### Using VSCode with DevContainer (Recommended)

1. **Install Prerequisites**
   - Docker Desktop
   - VSCode
   - Remote - Containers extension

2. **Clone and Open**
   ```bash
   git clone <repo-url>
   cd openfga_fdw
   code .
   ```

3. **Open in Container**
   - VSCode will show a prompt: "Reopen in Container"
   - Click to accept
   - Wait for container to build and compile

4. **Verify Setup**
   ```bash
   # In VSCode terminal
   which postgres
   pg_config --version
   ls -la /usr/lib/postgresql/18/lib/openfga_fdw.so
   ```

### Manual Local Setup (Linux/macOS)

1. **Install PostgreSQL 18 with Development Headers**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install postgresql-18 postgresql-server-dev-18

   # macOS
   brew install postgresql@18
   ```

2. **Install gRPC and Protobuf**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libgrpc++-dev libprotobuf-dev protobuf-compiler

   # macOS
   brew install grpc protobuf
   ```

3. **Build the Extension**
   ```bash
   cd openfga_fdw
   make USE_PGXS=1
   make install USE_PGXS=1
   ```

4. **Verify Installation**
   ```bash
   psql -c "CREATE EXTENSION openfga_fdw;"
   ```

## Building and Testing

### Build Commands

```bash
# Full clean build
make clean
make USE_PGXS=1

# Incremental build
make USE_PGXS=1

# Install built extension
make install USE_PGXS=1

# Clean only
make clean

# Display help
make help
```

### Test Suite

```bash
# Run all tests
make test

# Run specific test
make test TEST=cache_test

# Run with coverage
make test COVERAGE=1

# Run benchmarks
make benchmark
```

### Interactive Testing

```bash
# Start PostgreSQL with the extension
docker-compose -f docker/docker-compose.dev.yml up

# Connect in another terminal
psql postgresql://postgres:postgres@localhost:5432/openfga_test

# Test extension
CREATE EXTENSION openfga_fdw;
CREATE SERVER openfga_server FOREIGN DATA WRAPPER openfga_fdw
  OPTIONS (endpoint 'dns:///openfga:8081', store_id 'test');
CREATE FOREIGN TABLE acl_permission (
  object_type text, object_id text, subject_type text,
  subject_id text, relation text) SERVER openfga_server;

-- Test query
SELECT 1 FROM acl_permission LIMIT 1;
```

## Code Structure

### Source File Organization

```
src/
├── openfga_fdw.c          # Extension entry point (_PG_init, _PG_fini)
├── fdw_handler.c          # FDW callback implementations
├── fdw_validator.c        # Option validation logic
├── shared_memory.c        # Shared memory cache implementation
├── background_worker.c    # BGW main loop and OpenFGA sync
├── grpc_client.c          # gRPC client wrapper (C interface to C++)
├── guc.c                  # Configuration (GUC) handling
└── cache.c                # Cache utility functions

include/
├── openfga_fdw.h          # Main FDW interface
├── shared_memory.h        # Cache API
├── background_worker.h    # BGW API
├── grpc_client.h          # gRPC client API
└── guc.h                  # Configuration API

proto/
├── openfga.proto          # (Future) OpenFGA message definitions
└── openfga_grpc.proto     # (Future) OpenFGA service definitions

sql/
├── openfga_fdw.control    # Extension metadata
└── openfga_fdw--1.0.0.sql # DDL statements
```

### File Responsibilities

| File | Purpose | Key Functions |
|------|---------|---|
| openfga_fdw.c | Extension initialization | _PG_init, _PG_fini, openfga_fdw_handler |
| fdw_handler.c | Query execution | BeginScan, IterateScan, EndScan |
| fdw_validator.c | Validation | Validate server/table options |
| shared_memory.c | Cache operations | cache_lookup, cache_insert, generation tracking |
| background_worker.c | Sync logic | bgw_main, process_changes |
| grpc_client.c | OpenFGA communication | grpc_check_permission, grpc_read_changes |
| guc.c | Configuration | init_guc_variables, get_config |

## Coding Standards

### C Code Style

- **Indentation**: 4 spaces (no tabs)
- **Line Length**: Max 100 characters
- **Naming Conventions**:
  - Functions: `snake_case`
  - Structures: `CamelCase`
  - Constants: `UPPER_SNAKE_CASE`
  - Macros: `UPPER_SNAKE_CASE`

### Example Code Pattern

```c
/* Header comment with purpose */
static bool
check_permission_cached(const AclCacheKey *key, AclCacheEntry *entry)
{
    AclSharedState *state;
    bool found = false;

    /* Validate input */
    if (key == NULL || entry == NULL) {
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("key and entry must not be NULL")));
    }

    /* Get shared state */
    state = get_shared_state();
    if (state == NULL) {
        elog(WARNING, "shared state not initialized");
        return false;
    }

    /* Acquire lock and perform operation */
    LWLockAcquire(state->lock, LW_SHARED);
    {
        AclCacheEntry *cached = (AclCacheEntry *) hash_search(
            state->acl_cache,
            (void *) key,
            HASH_FIND,
            NULL);

        if (cached != NULL && !is_cache_stale(cached)) {
            *entry = *cached;
            found = true;
        }
    }
    LWLockRelease(state->lock);

    return found;
}
```

### Error Handling

```c
/* Proper error handling pattern */

/* For recoverable errors with user feedback */
if (invalid_option) {
    ereport(ERROR,
            (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
             errmsg("invalid option \"%s\"", option_name),
             errhint("valid options are: endpoint, store_id")));
}

/* For warnings */
if (potentially_problematic) {
    ereport(WARNING,
            (errmsg("unexpected value: %d", value)));
}

/* For debug logging */
elog(DEBUG2, "processing tuple: object_type=%s, object_id=%s",
     object_type, object_id);

/* For severe internal errors */
if (critical_error) {
    ereport(FATAL,
            (errcode(ERRCODE_INTERNAL_ERROR),
             errmsg("internal error in openfga_fdw")));
}
```

## Memory Management

### PostgreSQL Memory Context Rules

```c
/* Always use PostgreSQL memory context (palloc/pfree) */

/* Allocate memory in appropriate context */
MemoryContext old_context = MemoryContextSwitchTo(TopMemoryContext);
char *persistent_data = palloc(sizeof(char) * 100);
MemoryContextSwitchTo(old_context);

/* Automatic cleanup when context is destroyed */
void *temp_data = palloc(sizeof(int) * 1000);  /* auto-freed at transaction end */

/* Never use malloc/free directly */
// WRONG: void *ptr = malloc(100);
// RIGHT: void *ptr = palloc(100);
```

### Shared Memory Allocation

```c
/* Shared memory must be allocated during _PG_init or in a callback */

/* Request space upfront */
void _PG_init(void)
{
    RequestAddinShmemSpace(estimate_shared_memory());
}

/* Initialize after postmaster starts */
AclSharedState *get_shared_state(void)
{
    static AclSharedState *state = NULL;

    if (state == NULL) {
        bool found;
        state = ShmemInitStruct("openfga_fdw_state",
                               sizeof(AclSharedState),
                               &found);
        if (!found) {
            /* Initialize shared state */
        }
    }

    return state;
}
```

## Debugging

### Using GDB

```bash
# Start PostgreSQL under GDB
gdb --args postgres -D /var/lib/postgresql/data -F

# Set breakpoints before running
(gdb) break openfga_fdw_begin_scan
(gdb) run

# Common GDB commands
(gdb) continue             # Continue execution
(gdb) step                 # Step into function
(gdb) next                 # Step over function
(gdb) print variable       # Print variable value
(gdb) backtrace            # Show call stack
(gdb) frame N              # Switch to frame N
```

### Logging

```c
/* Enable debug logging in PostgreSQL */

/* In psql */
SET log_min_messages = DEBUG2;
SET log_statement = 'all';

/* In code */
elog(DEBUG1, "entering openfga_fdw_begin_scan");
elog(DEBUG2, "cache key: %s:%s", object_type, object_id);

/* View logs */
tail -f /var/log/postgresql/postgresql.log
```

### Valgrind (Memory Analysis)

```bash
# Build with debug symbols
CFLAGS="-g -O0" make USE_PGXS=1

# Run under Valgrind
valgrind --leak-check=full --show-leak-kinds=all \
    /usr/lib/postgresql/18/bin/postgres \
    -D /var/lib/postgresql/data -F
```

## Testing Strategy

### Unit Tests (Planned)

```c
/* Test structure */
static void
test_cache_lookup_hit(void)
{
    AclCacheKey key = {
        .object_type = "document",
        .object_id = "123",
        .subject_type = "user",
        .subject_id = "alice"
    };

    AclCacheEntry expected = {
        .key = key,
        .allow_bits = 0x01,
        .deny_bits = 0x00
    };

    /* Insert entry */
    cache_insert(&expected);

    /* Lookup and verify */
    AclCacheEntry result;
    assert(cache_lookup(&key, &result));
    assert(result.allow_bits == expected.allow_bits);
}
```

### Integration Tests

```sql
-- Test basic permission check
SELECT 1 FROM acl_permission
WHERE object_type = 'document'
  AND object_id = '123'
  AND subject_type = 'user'
  AND subject_id = 'alice'
  AND relation = 'read'
LIMIT 1;

-- Test cache hit statistics
SELECT * FROM openfga_fdw.cache_stats();
```

### Performance Tests

```sql
-- Test cache performance
EXPLAIN ANALYZE SELECT COUNT(*) FROM acl_permission
WHERE subject_type = 'user'
  AND subject_id = 'alice'
LIMIT 1000;
```

## Submitting Changes

### Before Committing

1. **Code Style Check**
   ```bash
   make clean
   make USE_PGXS=1
   # Check for warnings
   ```

2. **Build Successfully**
   ```bash
   make clean
   make USE_PGXS=1
   ```

3. **Test Changes**
   ```bash
   make test
   # Run interactive tests
   ```

4. **Update Documentation**
   - If API changes: Update README.md or ARCHITECTURE.md
   - If new features: Add to feature list
   - If bug fixes: Update CHANGELOG.md

### Commit Message Format

```
[TYPE] Brief description

Longer explanation if needed.

Relates to: #issue-number
Fixes: #bug-number
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `refactor`: Code restructuring
- `perf`: Performance improvement
- `test`: Test additions/modifications

**Examples**:
```
feat: Add cache preheating for frequently accessed permissions

Implements background preheating to reduce cold-start latency

Relates to: #42

fix: Correct generation counter overflow in subject tracking

Use atomic increments for thread safety

Fixes: #38

docs: Update README with cache tuning guide

Added performance tuning section for different workload patterns
```

## Common Development Tasks

### Adding a New GUC Parameter

1. **Declare in guc.h**
   ```c
   // In get_config() return structure
   bool new_parameter;
   ```

2. **Implement in guc.c**
   ```c
   static bool guc_new_parameter = false;

   void init_guc_variables(void) {
       DefineCustomBoolVariable(
           "openfga_fdw.new_parameter",
           "Description",
           "Help text",
           &guc_new_parameter,
           false,
           PGC_SUSET,
           0, NULL, NULL, NULL);
   }
   ```

3. **Use in code**
   ```c
   OpenFGAFdwConfig *cfg = get_config();
   if (cfg->new_parameter) { ... }
   ```

### Adding a New FDW Callback

1. **Declare in openfga_fdw.h**
   ```c
   void openfga_fdw_new_callback(ForeignScanState *node, ...);
   ```

2. **Implement in fdw_handler.c**
   ```c
   void openfga_fdw_new_callback(...) {
       elog(DEBUG1, "new_callback called");
       // Implementation
   }
   ```

3. **Register in openfga_fdw_handler()**
   ```c
   routine->NewCallback = openfga_fdw_new_callback;
   ```

### Adding a Cache Feature

1. **Define structure in shared_memory.h**
2. **Initialize in init_shared_memory() or get_shared_state()**
3. **Implement operations**
4. **Add tests**
5. **Document in ARCHITECTURE.md**

## Release Process

### Version Numbering

Follows semantic versioning: `MAJOR.MINOR.PATCH`

- **MAJOR**: Incompatible API changes
- **MINOR**: New functionality (backward compatible)
- **PATCH**: Bug fixes

### Release Checklist

- [ ] Update version in Makefile
- [ ] Update version in sql/openfga_fdw.control
- [ ] Update CHANGELOG.md
- [ ] Run full test suite
- [ ] Build production Docker image
- [ ] Tag commit: `git tag v1.0.0`
- [ ] Create GitHub release with binary artifacts

## Troubleshooting Development Issues

### Build Fails with "cannot find -lpq"

```bash
# Install PostgreSQL development libraries
sudo apt-get install postgresql-client-18 libpq-dev

# Or set PG_CONFIG explicitly
PG_CONFIG=/usr/lib/postgresql/18/bin/pg_config make USE_PGXS=1
```

### VSCode IntelliSense Not Working

```bash
# Regenerate compilation database
make clean
make USE_PGXS=1 VERBOSE=1 > compile_commands.json 2>&1

# Or create compile_commands.json manually
[
  {
    "directory": "/workspace",
    "command": "gcc -I/usr/include/postgresql/18/server -std=c11 -c src/openfga_fdw.c",
    "file": "src/openfga_fdw.c"
  }
]
```

### Tests Fail with "extension not found"

```bash
# Make sure extension is installed
sudo make install USE_PGXS=1

# Check installation
ls -la $(pg_config --libdir)/openfga_fdw.so

# Restart PostgreSQL
sudo systemctl restart postgresql
```

## Performance Profiling

### CPU Profiling

```bash
# Build with profiling support
CFLAGS="-pg" make USE_PGXS=1

# Run profiled workload
gprof /usr/lib/postgresql/18/bin/postgres gmon.out
```

### Memory Profiling

```bash
# Use Valgrind for detailed analysis
valgrind --tool=massif postgres -D /var/lib/postgresql/data -F

# Analyze results
ms_print massif.out.<pid>
```

## Contributing

See CONTRIBUTING.md (if available) or follow GitHub contribution guidelines.

---

Happy coding! For questions or issues, please open a GitHub issue or contact the maintainers.
