# PostFGA Extension - Installation and Testing Guide

This guide explains how to install and test the PostFGA extension in your PostgreSQL database.

## Prerequisites

- PostgreSQL 18 (or compatible version)
- Build completed (run `cmake --build build` if not done yet)
- Sufficient permissions to install PostgreSQL extensions

## Quick Start (Automated)

The easiest way to install and test the extension is using the automated script:

```bash
# From project root
./scripts/install_and_test.sh
```

This script will:
1. Check PostgreSQL installation
2. Build the extension (if needed)
3. Initialize PostgreSQL
4. Install extension files
5. Create the extension in the test database
6. Run the test suite

## Manual Installation

If you prefer to install manually or need more control:

### Step 1: Build the Extension

```bash
# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Return to project root
cd ..
```

### Step 2: Initialize PostgreSQL

```bash
./scripts/init_postgres.sh
```

This will:
- Check if PostgreSQL is running
- Create test database (`postfga_test` by default)
- Add configuration template to `postgresql.conf`

### Step 3: Install Extension Files

```bash
./scripts/install_extension.sh
```

This copies:
- `build/postfga.so` → PostgreSQL lib directory
- `sql/postfga.control` → PostgreSQL extension directory
- `sql/postfga--1.0.0.sql` → PostgreSQL extension directory

### Step 4: Create Extension in Database

```bash
# Connect to database
sudo -u postgres psql postfga_test

# In psql:
CREATE EXTENSION postfga;
```

### Step 5: Run Tests

```bash
# From psql
\i tests/test_extension.sql
```

Or from command line:
```bash
sudo -u postgres psql -d postfga_test -f tests/test_extension.sql
```

## Testing the Extension

### Basic Functionality Test

The test suite (`tests/test_extension.sql`) verifies:

1. ✅ Extension creation
2. ✅ FDW (Foreign Data Wrapper) setup
3. ✅ Function availability
4. ✅ Server creation
5. ✅ Foreign table creation
6. ✅ Cache management functions

### Example Usage

```sql
-- 1. Create a foreign server
CREATE SERVER my_openfga_server
    FOREIGN DATA WRAPPER postfga_fdw
    OPTIONS (
        endpoint 'dns:///localhost:8081',
        store_id 'your-store-id',
        use_grpc 'true'
    );

-- 2. Create foreign table for ACL permissions
CREATE FOREIGN TABLE acl_permission (
    object_type   text,
    object_id     text,
    subject_type  text,
    subject_id    text,
    relation      text
) SERVER my_openfga_server;

-- 3. Check if user has permission
SELECT 1 FROM acl_permission
WHERE object_type  = 'document'
  AND object_id    = '123'
  AND subject_type = 'user'
  AND subject_id   = 'alice'
  AND relation     = 'read';

-- Result:
--   1 row  = permission granted (allow)
--   0 rows = permission denied (deny)
```

### Cache Management

```sql
-- View cache statistics
SELECT * FROM postfga_fdw.cache_stats();

-- Clear cache
SELECT postfga_fdw.clear_cache();
```

## Configuration

### GUC Parameters (postgresql.conf)

The extension supports the following configuration parameters:

```ini
# OpenFGA connection
postfga_fdw.endpoint = 'dns:///localhost:8081'
postfga_fdw.store_id = 'your-store-id'
postfga_fdw.authorization_model_id = 'optional-model-id'

# Relation definitions (comma-separated)
postfga_fdw.relations = 'read,write,edit,delete,download,owner'

# Cache settings
postfga_fdw.cache_ttl_ms = 60000
postfga_fdw.max_cache_entries = 10000

# Background worker settings
postfga_fdw.bgw_workers = 1

# Behavior options
postfga_fdw.fallback_to_grpc_on_miss = true
```

Edit `/etc/postgresql/18/main/postgresql.conf` and restart PostgreSQL:

```bash
sudo systemctl restart postgresql
```

### Per-Server Options

You can also specify options per foreign server:

```sql
CREATE SERVER my_openfga
    FOREIGN DATA WRAPPER postfga_fdw
    OPTIONS (
        endpoint 'dns:///openfga-prod:8081',
        store_id 'prod-store-id',
        use_grpc 'true',
        connect_timeout_ms '5000',
        request_timeout_ms '3000'
    );
```

## Troubleshooting

### Extension Not Loading

1. Check PostgreSQL logs:
   ```bash
   sudo tail -f /var/log/postgresql/postgresql-18-main.log
   ```

2. Verify files are installed:
   ```bash
   pg_config --pkglibdir  # Should contain postfga.so
   pg_config --sharedir   # extension/ should contain postfga.control
   ```

3. Check permissions:
   ```bash
   ls -l $(pg_config --pkglibdir)/postfga.so
   ls -l $(pg_config --sharedir)/extension/postfga*
   ```

### Query Failures

If queries to `acl_permission` fail:

1. ✅ Ensure OpenFGA is running and accessible
2. ✅ Verify endpoint configuration
3. ✅ Check store_id is correct
4. ✅ Verify network connectivity to OpenFGA

Expected error without OpenFGA:
```
ERROR:  could not connect to OpenFGA server
HINT:  Check endpoint and ensure OpenFGA is running
```

### Cache Issues

If cache seems stale:

```sql
-- Clear cache manually
SELECT postfga_fdw.clear_cache();

-- Check cache statistics
SELECT * FROM postfga_fdw.cache_stats();
```

## Development Workflow

### Rebuilding After Changes

```bash
# Rebuild
cmake --build build

# Reinstall
./scripts/install_extension.sh

# Reload in PostgreSQL
sudo -u postgres psql postfga_test -c "DROP EXTENSION postfga CASCADE; CREATE EXTENSION postfga;"
```

### Running Tests After Changes

```bash
sudo -u postgres psql -d postfga_test -f tests/test_extension.sql
```

## DevContainer Usage

If using the devcontainer:

1. Open project in VSCode with Remote-Containers
2. Container will have PostgreSQL pre-installed
3. Run installation script:
   ```bash
   ./scripts/install_and_test.sh
   ```

## Next Steps

1. **Configure OpenFGA**: Set up OpenFGA server with your authorization model
2. **Define Relations**: Configure the relations in GUC or server options
3. **Create Foreign Tables**: Set up tables for your ACL needs
4. **Test Permissions**: Query the foreign tables to verify permission checks
5. **Monitor Cache**: Use cache stats to optimize performance

## Additional Resources

- [Project Documentation](README.md)
- [Architecture Overview](ARCHITECTURE.md)
- [Development Guide](DEVELOPMENT.md)
- [OpenFGA Documentation](https://openfga.dev/)

## Support

For issues or questions:
- Check PostgreSQL logs
- Review test output
- Verify OpenFGA connectivity
- Check configuration parameters
