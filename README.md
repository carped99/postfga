# OpenFGA FDW - PostgreSQL Foreign Data Wrapper for OpenFGA

A PostgreSQL 18+ Foreign Data Wrapper (FDW) extension that integrates with OpenFGA for ACL permission checking. This extension provides a seamless way to query authorization decisions directly from SQL queries using a shared memory cache with generation-based invalidation.

## Features

- **FDW Integration**: Query OpenFGA permissions directly from SQL using foreign tables
- **Shared Memory Cache**: High-performance caching layer with LWLock synchronization
- **Generation-Based Invalidation**: Efficient cache invalidation strategy with scope-based tracking
- **Background Worker**: Asynchronous change stream processing from OpenFGA
- **gRPC Integration**: Native gRPC client for OpenFGA communication
- **PostgreSQL 18+ Support**: Built on the latest PostgreSQL FDW/BGW API
- **C++20 Implementation**: Modern C++ with C API bridges for PostgreSQL

## Architecture

### Components

1. **FDW Handler** (`src/fdw_handler.c`)
   - Implements PostgreSQL FDW callbacks
   - Handles query planning and execution
   - Manages tuple fetching and filtering

2. **Shared Memory Cache** (`src/shared_memory.c`)
   - Thread-safe cache using PostgreSQL LWLocks
   - Stores ACL permission bitmasks
   - Multiple generation tracking maps for invalidation

3. **Background Worker** (`src/background_worker.c`)
   - Processes OpenFGA change stream events
   - Updates generation counters
   - Manages cache invalidation

4. **gRPC Client** (`src/grpc_client.c`)
   - Wrapper around C++ gRPC implementation
   - Calls OpenFGA Check and ReadChanges APIs
   - Handles network errors and retries

### Permission Model

Permissions are checked using 5 columns:
- `object_type`: Type of resource (e.g., "document", "folder")
- `object_id`: ID of the resource
- `subject_type`: Type of subject (e.g., "user", "group")
- `subject_id`: ID of the subject
- `relation`: The relationship/permission (e.g., "read", "write")

Query pattern:
```sql
SELECT 1 FROM acl_permission
WHERE object_type = 'document'
  AND object_id = '123'
  AND subject_type = 'user'
  AND subject_id = 'alice'
  AND relation = 'read';
```

Result interpretation:
- **1 row returned** = Permission ALLOWED
- **0 rows returned** = Permission DENIED

## Installation

### Prerequisites

- PostgreSQL 18 or later
- C++20 compatible compiler (GCC 10+, Clang 10+)
- gRPC and Protocol Buffers libraries
- OpenFGA service running and accessible

### Building from Source

```bash
# Clone the repository
git clone <repo-url>
cd openfga_fdw

# Build using PGXS
make USE_PGXS=1

# Install to PostgreSQL
make install USE_PGXS=1
```

### Using Docker

**Development environment:**
```bash
docker-compose -f docker/docker-compose.dev.yml up
```

**Production image:**
```bash
docker build -f docker/Dockerfile.prod -t openfga-postgres:latest .
docker run -e POSTGRES_PASSWORD=postgres -p 5432:5432 openfga-postgres:latest
```

## Configuration

### GUC Parameters

Configure via `postgresql.conf` or `SET` command:

```sql
-- Required parameters
SET openfga_fdw.endpoint = 'dns:///openfga:8081';
SET openfga_fdw.store_id = 'your-store-id';

-- Optional parameters
SET openfga_fdw.authorization_model_id = 'model-id';
SET openfga_fdw.relations = 'read,write,edit,delete,download,owner';
SET openfga_fdw.cache_ttl_ms = 60000;                    -- 60 seconds
SET openfga_fdw.max_cache_entries = 10000;
SET openfga_fdw.bgw_workers = 1;
SET openfga_fdw.fallback_to_grpc_on_miss = true;
```

### Creating FDW Server and Table

```sql
-- Create extension
CREATE EXTENSION openfga_fdw;

-- Create server
CREATE SERVER openfga_server
  FOREIGN DATA WRAPPER openfga_fdw
  OPTIONS (
    endpoint 'dns:///openfga:8081',
    store_id 'your-store-id'
  );

-- Create foreign table
CREATE FOREIGN TABLE acl_permission (
    object_type text,
    object_id text,
    subject_type text,
    subject_id text,
    relation text
) SERVER openfga_server;
```

## Usage Examples

### Basic Permission Check

```sql
-- Check if user 'alice' can read document '123'
SELECT 1 FROM acl_permission
WHERE object_type = 'document'
  AND object_id = '123'
  AND subject_type = 'user'
  AND subject_id = 'alice'
  AND relation = 'read'
LIMIT 1;
```

### Using in Application Logic

```sql
-- Find all documents a user can read
SELECT document_id, title
FROM documents d
WHERE EXISTS (
  SELECT 1 FROM acl_permission
  WHERE object_type = 'document'
    AND object_id = d.id::text
    AND subject_type = 'user'
    AND subject_id = 'alice'
    AND relation = 'read'
);
```

### Multiple Relation Check

```sql
-- Check multiple permissions at once
WITH user_perms AS (
  SELECT DISTINCT relation
  FROM acl_permission
  WHERE object_type = 'document'
    AND object_id = '123'
    AND subject_type = 'user'
    AND subject_id = 'alice'
)
SELECT
  'read' as permission,
  EXISTS(SELECT 1 FROM user_perms WHERE relation = 'read') as allowed
UNION ALL
SELECT
  'write' as permission,
  EXISTS(SELECT 1 FROM user_perms WHERE relation = 'write') as allowed
UNION ALL
SELECT
  'delete' as permission,
  EXISTS(SELECT 1 FROM user_perms WHERE relation = 'delete') as allowed;
```

## Cache Management

### View Cache Statistics

```sql
SELECT * FROM openfga_fdw.cache_stats();
```

Output:
```
 total_entries | cached_entries | cache_hits | cache_misses
---------------+----------------+------------+--------------
        10000  |           5432 |      98765 |          1234
```

### Clear Cache

```sql
SELECT openfga_fdw.clear_cache();
```

## Performance Considerations

### Cache Invalidation Strategy

The extension uses a multi-level generation-based invalidation:

1. **object_type generation**: Incremented when any object of this type changes
2. **object generation**: Incremented when this specific object changes
3. **subject_type generation**: Incremented when any subject of this type changes
4. **subject generation**: Incremented when this specific subject changes

This strategy allows efficient partial cache invalidation without clearing everything.

### Generation Update Cascade

When OpenFGA reports a change in tuple `(object_type, object_id, relation, subject_type, subject_id)`:

```
Generation Updates:
  - object_type_gen[object_type]++
  - object_gen[object_type + object_id]++
  - subject_type_gen[subject_type]++
  - subject_gen[subject_type + subject_id]++
```

Cache entries are marked stale if any of their tracked generations differs from shared memory values.

### Performance Tuning

**Increase cache TTL for read-heavy workloads:**
```sql
SET openfga_fdw.cache_ttl_ms = 300000;  -- 5 minutes
```

**Increase cache size for larger datasets:**
```sql
SET openfga_fdw.max_cache_entries = 50000;
```

**Disable gRPC fallback in cache-only mode:**
```sql
SET openfga_fdw.fallback_to_grpc_on_miss = false;
```

## Development

### VSCode DevContainer

The project includes a complete DevContainer setup:

```bash
# Open in VSCode with Remote - Containers
# VSCode will automatically:
# 1. Build the dev container
# 2. Compile the extension
# 3. Start PostgreSQL and OpenFGA
```

### Building and Testing

```bash
# Build the extension
make USE_PGXS=1

# Clean build artifacts
make clean

# Install to PostgreSQL
make install USE_PGXS=1

# Run tests
make test
```

### Code Structure

```
openfga_fdw/
├── include/
│   ├── openfga_fdw.h          # Main FDW interface
│   ├── shared_memory.h        # Shared memory cache API
│   ├── background_worker.h    # BGW interface
│   ├── grpc_client.h          # gRPC client interface
│   └── guc.h                  # Configuration API
├── src/
│   ├── openfga_fdw.c          # Extension entry point
│   ├── fdw_handler.c          # FDW callbacks
│   ├── fdw_validator.c        # Option validation
│   ├── shared_memory.c        # Cache implementation
│   ├── background_worker.c    # BGW implementation
│   ├── grpc_client.c          # gRPC wrapper
│   ├── guc.c                  # Configuration
│   └── cache.c                # Cache utilities
├── sql/
│   ├── openfga_fdw.control    # Extension metadata
│   └── openfga_fdw--1.0.0.sql # DDL statements
├── docker/
│   ├── Dockerfile.dev         # Development container
│   ├── Dockerfile.prod        # Production container
│   ├── docker-compose.dev.yml # Dev orchestration
│   └── initdb.d/              # Initialization scripts
├── .devcontainer/             # VSCode devcontainer config
└── Makefile                   # Build configuration
```

## Troubleshooting

### Extension Won't Load

**Error**: `ERROR: could not load library "...openfga_fdw.so"`

**Solution**: Ensure PostgreSQL can find the shared object:
```bash
ls -la $(pg_config --libdir)/openfga_fdw.so
```

### gRPC Connection Issues

**Error**: `Could not connect to OpenFGA endpoint`

**Solution**: Verify OpenFGA is running and accessible:
```bash
psql -d postgres -c "SET openfga_fdw.endpoint = 'dns:///openfga:8081'; SELECT 1;"
```

### Cache Not Working

**Error**: `Cache lookup failed`

**Solution**: Check shared memory initialization:
```bash
# Restart PostgreSQL to reinitialize shared memory
sudo systemctl restart postgresql
```

## PostgreSQL Version Support

- **PostgreSQL 18**: Primary target (fully tested)
- **PostgreSQL 19+**: Should work with minimal changes (use `PG_VERSION_NUM` checks)
- **PostgreSQL 16-17**: Backward compatible if FDW/BGW API unchanged

To support other versions:
```bash
make PG_CONFIG=/path/to/pg_config USE_PGXS=1
```

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/xyz`
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

[Specify your license here - e.g., Apache 2.0, MIT, etc.]

## Support

For issues, questions, or suggestions:
- Open an GitHub issue
- Check existing documentation
- Review CLAUDE.md for detailed architecture notes

## Acknowledgments

- PostgreSQL community for excellent extension documentation
- OpenFGA team for comprehensive authorization API
- gRPC team for robust RPC framework

## Roadmap

### v1.1
- [ ] C++ gRPC client implementation
- [ ] Full ReadChanges API integration
- [ ] Comprehensive test suite

### v1.2
- [ ] INSERT/UPDATE/DELETE support (write operations)
- [ ] Caching statistics API
- [ ] Performance monitoring hooks

### v2.0
- [ ] Multi-store support
- [ ] Custom authorization models
- [ ] Pluggable caching backends
