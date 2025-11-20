# Getting Started with OpenFGA FDW

This guide will help you get up and running with the OpenFGA FDW extension in minutes.

## Quick Start (Docker)

### Prerequisites
- Docker and Docker Compose installed
- OpenFGA instance (we'll start one with compose)

### 1. Start the Development Environment

```bash
cd openfga_fdw
docker-compose -f docker/docker-compose.dev.yml up
```

This starts:
- PostgreSQL 18 with openfga_fdw extension
- OpenFGA service
- Both fully initialized and ready to use

### 2. Connect to PostgreSQL

```bash
# From another terminal
docker-compose -f docker/docker-compose.dev.yml exec postgres psql -U postgres
```

### 3. Verify Extension is Loaded

```sql
-- Check if extension exists
SELECT * FROM pg_extension WHERE extname = 'openfga_fdw';

-- Check if FDW is available
SELECT * FROM pg_foreign_data_wrapper WHERE fdwname = 'openfga_fdw';
```

### 4. Create Your First Query

```sql
-- Check if foreign table exists (auto-created by init script)
\d acl_permission

-- Test a permission check
SELECT 1 FROM acl_permission
WHERE object_type = 'document'
  AND object_id = 'doc-1'
  AND subject_type = 'user'
  AND subject_id = 'alice'
  AND relation = 'read'
LIMIT 1;
```

## Traditional Installation (Source)

### Prerequisites
- PostgreSQL 18+ development files
- GCC 10+ or Clang 10+
- gRPC development libraries
- OpenFGA service running

### 1. Install Dependencies (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
    postgresql-18 \
    postgresql-server-dev-18 \
    build-essential \
    cmake \
    libgrpc++-dev \
    libprotobuf-dev
```

### 2. Clone and Build

```bash
git clone <repository-url>
cd openfga_fdw

# Build
make USE_PGXS=1

# Install
sudo make install USE_PGXS=1
```

### 3. Configure PostgreSQL

Add to `postgresql.conf`:

```ini
# GUC parameters for openfga_fdw
openfga_fdw.endpoint = 'dns:///localhost:8081'
openfga_fdw.store_id = 'your-store-id'
openfga_fdw.relations = 'read,write,edit,delete,download,owner'
openfga_fdw.cache_ttl_ms = 60000
openfga_fdw.fallback_to_grpc_on_miss = true
```

### 4. Create Extension

```bash
# Connect as superuser
sudo -u postgres psql -d postgres

-- Create the extension
CREATE EXTENSION openfga_fdw;

-- Verify
\dx openfga_fdw
```

## Setting Up OpenFGA

### Option 1: Docker (Easiest)

```bash
docker run -d \
  --name openfga \
  -p 8080:8080 \
  -p 8081:8081 \
  -p 3000:3000 \
  openfga/openfga:latest run
```

### Option 2: Locally

Download from [openfga.dev](https://openfga.dev) and run:

```bash
./openfga run
```

### Initialize Store

```bash
# Get your store ID
STORE_ID=$(curl -X POST http://localhost:8080/stores \
  -H "Content-Type: application/json" \
  -d '{}' | jq -r '.id')

echo "Store ID: $STORE_ID"
```

## Create Sample Authorization Model

OpenFGA uses an authorization model to define relationships. Here's a basic example:

```yaml
# model.yaml
schema 1.1

type user
type document
    relations
    define read: [user]
    define write: [user]
    define delete: [user]
```

Upload it via OpenFGA Admin UI at `http://localhost:3000/` or via API.

## Running Your First Queries

### Setup PostgreSQL FDW

```sql
-- Create server
CREATE SERVER openfga_server
  FOREIGN DATA WRAPPER openfga_fdw
  OPTIONS (
    endpoint 'dns:///localhost:8081',
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

### Add Sample Relationships in OpenFGA

Use the OpenFGA API or Admin UI to create some test relationships:

```bash
# Example: Grant alice read access to document1
curl -X POST http://localhost:8080/stores/{STORE_ID}/write \
  -H "Content-Type: application/json" \
  -d '{
    "writes": [
      {
        "object": "document:document1",
        "relation": "read",
        "subject": "user:alice"
      }
    ]
  }'
```

### Test Permission Checks

```sql
-- Check if alice can read document1
SELECT 1 FROM acl_permission
WHERE object_type = 'document'
  AND object_id = 'document1'
  AND subject_type = 'user'
  AND subject_id = 'alice'
  AND relation = 'read'
LIMIT 1;

-- Result: 1 row (permission granted)
```

### Check Denied Permissions

```sql
-- Check if bob can read document1 (he can't)
SELECT 1 FROM acl_permission
WHERE object_type = 'document'
  AND object_id = 'document1'
  AND subject_type = 'user'
  AND subject_id = 'bob'
  AND relation = 'read'
LIMIT 1;

-- Result: 0 rows (permission denied)
```

## Practical Example: Application Use Case

### Real-World Scenario

A document management system needs to check if users have read/write access to documents.

### Step 1: OpenFGA Setup

```bash
# Create authorization model
curl -X POST http://localhost:8080/stores/{STORE_ID}/write_authorization_model \
  -H "Content-Type: application/json" \
  -d '{
    "type_definitions": [
      {
        "type": "user"
      },
      {
        "type": "document",
        "relations": {
          "owner": {"union": {"child": [{"this": {}}]}},
          "writer": {"union": {"child": [{"computedUserset": {"object": "", "relation": "owner"}}]}},
          "reader": {"union": {"child": [{"computedUserset": {"object": "", "relation": "writer"}}]}}
        }
      }
    ]
  }'
```

### Step 2: PostgreSQL Setup

```sql
-- Create users table
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username text NOT NULL UNIQUE
);

-- Create documents table
CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    title text NOT NULL,
    owner_id integer REFERENCES users(id)
);

-- Insert sample data
INSERT INTO users (username) VALUES ('alice'), ('bob'), ('charlie');
INSERT INTO documents (title, owner_id) VALUES
    ('Project Plan', 1),      -- alice owns
    ('Budget Spreadsheet', 2); -- bob owns

-- Grant permissions in OpenFGA
-- (Done via OpenFGA API or this application)
```

### Step 3: Application Query

```sql
-- Find documents that user 'alice' can read
SELECT d.id, d.title, u.username
FROM documents d
JOIN users u ON d.owner_id = u.id
WHERE EXISTS (
    SELECT 1 FROM acl_permission
    WHERE object_type = 'document'
      AND object_id = d.id::text
      AND subject_type = 'user'
      AND subject_id = 'alice'
      AND relation = 'reader'
);
```

## Performance Tuning

### For Read-Heavy Workloads

```sql
-- Increase cache TTL (documents don't change often)
SET openfga_fdw.cache_ttl_ms = 300000;  -- 5 minutes

-- Increase cache size
SET openfga_fdw.max_cache_entries = 50000;
```

### For Frequently-Changing Permissions

```sql
-- Lower cache TTL (permissions change frequently)
SET openfga_fdw.cache_ttl_ms = 10000;  -- 10 seconds

-- Ensure BGW is running (handles invalidation)
SELECT * FROM pg_stat_activity WHERE backend_type = 'background worker';
```

### Cache-Only Mode (High Performance)

```sql
-- Disable gRPC fallback for maximum speed
-- Cache misses will return DENY instead of querying OpenFGA
SET openfga_fdw.fallback_to_grpc_on_miss = false;
```

## Troubleshooting

### Extension Won't Load

```bash
# Check if the .so file is in the right place
ls -la $(pg_config --libdir)/openfga_fdw.so

# If missing, rebuild and install
make clean
make USE_PGXS=1
make install USE_PGXS=1

# Restart PostgreSQL
sudo systemctl restart postgresql
```

### Can't Connect to OpenFGA

```bash
# Check if OpenFGA is running
curl http://localhost:8080/health

# Verify endpoint in GUC
SELECT current_setting('openfga_fdw.endpoint');

# Test gRPC endpoint directly
grpcurl -plaintext localhost:8081 list
```

### All Queries Return Empty (No Permissions)

```sql
-- Make sure relationships are created in OpenFGA
-- Check the store ID matches
SELECT current_setting('openfga_fdw.store_id');

-- Verify relationships via OpenFGA API
curl http://localhost:8080/stores/{STORE_ID}/read
```

### High Query Latency

```sql
-- Check cache statistics
SELECT * FROM openfga_fdw.cache_stats();

-- If cache_hits is low, increase TTL or cache size
SET openfga_fdw.cache_ttl_ms = 120000;

-- Monitor BGW performance
SELECT * FROM pg_stat_activity
WHERE backend_type = 'background worker'
  AND query LIKE '%openfga%';
```

## Next Steps

1. **Explore ARCHITECTURE.md** for detailed design
2. **Read README.md** for comprehensive documentation
3. **Check examples/** directory for more use cases
4. **Review GUC parameters** in README.md for tuning

## Getting Help

- Check README.md for detailed documentation
- Review ARCHITECTURE.md for implementation details
- See examples/ directory for usage patterns
- Open an issue on GitHub for bugs

## Development

For development with VSCode:

```bash
# Open in VSCode with Remote Containers
code .

# VSCode will auto-setup the devcontainer
# Build will complete automatically
```

Once VSCode devcontainer is ready:

```bash
# Build
make USE_PGXS=1

# Debug with gdb
gdb --args /usr/lib/postgresql/18/bin/postgres -D /var/lib/postgresql/data -F
```

---

Congratulations! You're now ready to use OpenFGA FDW. Start by creating some relationships in OpenFGA and querying permissions from PostgreSQL.
