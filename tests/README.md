# PostFGA Extension Tests

This directory contains test scripts for the PostFGA extension.

## Test Files

### `test_extension.sql`

Comprehensive test suite that verifies:

1. **Extension Creation**
   - Creates the `postfga` extension
   - Verifies extension is registered in `pg_extension`

2. **FDW Setup**
   - Checks that `postfga_fdw` foreign data wrapper is created
   - Verifies FDW handler and validator functions exist

3. **Function Verification**
   - `postfga_fdw_handler` - FDW handler function
   - `postfga_fdw_validator` - FDW validator function
   - `clear_cache()` - Cache clearing function
   - `cache_stats()` - Cache statistics function

4. **Server Creation**
   - Creates a test foreign server with sample configuration
   - Verifies server options are stored correctly

5. **Foreign Table Creation**
   - Creates `acl_permission` foreign table
   - Verifies table structure

6. **Cache Management**
   - Tests cache clearing
   - Tests cache statistics retrieval

7. **Query Structure**
   - Attempts a sample permission query
   - Handles expected failures gracefully when OpenFGA is not running

## Running Tests

### Quick Test

```bash
# From project root
./scripts/install_and_test.sh
```

### Manual Test Execution

```bash
# Connect to test database and run tests
sudo -u postgres psql -d postfga_test -f tests/test_extension.sql
```

### In psql

```sql
-- Connect to database
\c postfga_test

-- Run test file
\i tests/test_extension.sql
```

## Test Output

Expected output includes:

```
=== PostFGA Extension Test Suite ===

Step 1: Creating extension...
DROP EXTENSION
CREATE EXTENSION

Step 2: Verifying extension...
 extname | extversion
---------+------------
 postfga | 1.0.0

Step 3: Verifying FDW...
  fdwname
------------
 postfga_fdw

Step 4: Verifying functions...
        proname         | pronargs
------------------------+----------
 cache_stats            |        0
 clear_cache            |        0
 postfga_fdw_handler    |        0
 postfga_fdw_validator  |        2

... (continued)

=== Test Suite Complete ===

Summary:
  - Extension created: postfga
  - FDW created: postfga_fdw
  - Test server created: test_openfga_server
  - Foreign table created: acl_permission
  - Cache functions working: clear_cache(), cache_stats()
```

## Test Expectations

### When OpenFGA is NOT Running

- Most tests will **PASS** ✅
- Query structure tests will show a **WARNING** ⚠️
  ```
  NOTICE:  Query failed (expected if OpenFGA not running): could not connect...
  ```
- This is **normal** and expected for structural tests

### When OpenFGA IS Running

- All tests should **PASS** ✅
- Query tests will execute successfully
- Cache statistics will show actual data

## Adding New Tests

To add new test cases:

1. Create a new `.sql` file in this directory
2. Follow the pattern in `test_extension.sql`:
   ```sql
   \echo 'Test Name'
   -- Test SQL here
   ```
3. Handle expected failures gracefully:
   ```sql
   DO $$
   BEGIN
       BEGIN
           -- Your test code
       EXCEPTION WHEN OTHERS THEN
           RAISE NOTICE 'Expected failure: %', SQLERRM;
       END;
   END $$;
   ```

## Integration Tests

For integration tests with OpenFGA:

1. **Prerequisites:**
   - OpenFGA server running
   - Store created with authorization model
   - Test data loaded

2. **Configuration:**
   - Update server endpoint in test scripts
   - Set correct `store_id`
   - Configure relations to match your model

3. **Run:**
   ```bash
   psql -d postfga_test -f tests/integration_test.sql
   ```

## Performance Tests

For performance testing:

```sql
-- Enable timing
\timing on

-- Run multiple queries
SELECT COUNT(*) FROM (
    SELECT 1 FROM acl_permission
    WHERE object_type = 'document'
      AND object_id = 'test-' || generate_series(1, 1000)
      AND subject_type = 'user'
      AND subject_id = 'alice'
      AND relation = 'read'
) AS results;

-- Check cache statistics
SELECT * FROM postfga_fdw.cache_stats();
```

## CI/CD Integration

Tests can be automated in CI/CD pipelines:

```bash
#!/bin/bash
# ci-test.sh

# Start PostgreSQL
service postgresql start

# Install extension
./scripts/install_and_test.sh

# Exit with test result code
exit $?
```

## Troubleshooting Tests

### Tests Fail to Run

1. Check extension is installed:
   ```sql
   SELECT * FROM pg_extension WHERE extname = 'postfga';
   ```

2. Check files exist:
   ```bash
   ls -l $(pg_config --pkglibdir)/postfga.so
   ls -l $(pg_config --sharedir)/extension/postfga*
   ```

3. Check PostgreSQL logs:
   ```bash
   tail -f /var/log/postgresql/postgresql-*.log
   ```

### Unexpected Failures

1. Clear cache and retry:
   ```sql
   SELECT postfga_fdw.clear_cache();
   ```

2. Drop and recreate extension:
   ```sql
   DROP EXTENSION IF EXISTS postfga CASCADE;
   CREATE EXTENSION postfga;
   ```

3. Check for conflicting extensions:
   ```sql
   SELECT extname FROM pg_extension;
   ```

## Test Coverage

Current test coverage includes:

- ✅ Extension installation
- ✅ FDW creation
- ✅ Function registration
- ✅ Server configuration
- ✅ Table creation
- ✅ Cache management
- ✅ Basic query structure
- ⏳ Full query execution (requires OpenFGA)
- ⏳ Cache invalidation (requires OpenFGA changes)
- ⏳ Background worker operation (future)

## Future Tests

Planned test additions:

1. **Cache Behavior**
   - Cache hit/miss ratios
   - TTL expiration
   - Generation-based invalidation

2. **Concurrent Operations**
   - Multiple simultaneous queries
   - Cache contention
   - Lock behavior

3. **Error Handling**
   - Network failures
   - Invalid configurations
   - Malformed queries

4. **Performance**
   - Query latency
   - Cache efficiency
   - Memory usage

5. **Integration**
   - Full OpenFGA workflow
   - Real permission checks
   - Change stream processing
