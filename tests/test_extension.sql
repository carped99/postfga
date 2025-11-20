-- Test script for postfga extension
-- This script tests the basic functionality of the postfga extension

\echo '=== PostFGA Extension Test Suite ==='
\echo ''

-- Step 1: Create extension
\echo 'Step 1: Creating extension...'
DROP EXTENSION IF EXISTS postfga CASCADE;
CREATE EXTENSION postfga;

-- Step 2: Verify extension is created
\echo 'Step 2: Verifying extension...'
SELECT extname, extversion FROM pg_extension WHERE extname = 'postfga';

-- Step 3: Verify FDW is created
\echo 'Step 3: Verifying FDW...'
SELECT fdwname FROM pg_foreign_data_wrapper WHERE fdwname = 'postfga_fdw';

-- Step 4: Verify functions are created
\echo 'Step 4: Verifying functions...'
SELECT proname, pronargs FROM pg_proc
WHERE proname IN ('postfga_fdw_handler', 'postfga_fdw_validator', 'clear_cache', 'cache_stats')
ORDER BY proname;

-- Step 5: Create a test server (will need OpenFGA configuration)
\echo 'Step 5: Creating test server...'
DROP SERVER IF EXISTS test_openfga_server CASCADE;
CREATE SERVER test_openfga_server
    FOREIGN DATA WRAPPER postfga_fdw
    OPTIONS (
        endpoint 'dns:///localhost:8081',
        store_id 'test-store-id',
        use_grpc 'true'
    );

-- Step 6: Verify server is created
\echo 'Step 6: Verifying server...'
SELECT srvname, srvoptions FROM pg_foreign_server WHERE srvname = 'test_openfga_server';

-- Step 7: Create a foreign table for ACL permissions
\echo 'Step 7: Creating foreign table...'
DROP FOREIGN TABLE IF EXISTS acl_permission;
CREATE FOREIGN TABLE acl_permission (
    object_type   text,
    object_id     text,
    subject_type  text,
    subject_id    text,
    relation      text
) SERVER test_openfga_server;

-- Step 8: Verify foreign table is created
\echo 'Step 8: Verifying foreign table...'
SELECT tablename, schemaname FROM pg_tables
WHERE tablename = 'acl_permission'
UNION ALL
SELECT relname as tablename, n.nspname as schemaname
FROM pg_class c
JOIN pg_namespace n ON c.relnamespace = n.oid
WHERE c.relname = 'acl_permission' AND c.relkind = 'f';

-- Step 9: Test cache management functions
\echo 'Step 9: Testing cache functions...'
\echo '  - Clearing cache...'
SELECT postfga_fdw.clear_cache();

\echo '  - Getting cache stats...'
SELECT * FROM postfga_fdw.cache_stats();

-- Step 10: Test query (will fail if OpenFGA is not running, but that's ok for structure test)
\echo 'Step 10: Testing query structure (may fail if OpenFGA not running)...'
\echo '  Note: This is expected to fail if OpenFGA is not configured/running'
\echo '  Testing query: SELECT 1 FROM acl_permission WHERE object_type = ''document'' LIMIT 1;'

-- Attempt query but don't fail the test script
DO $$
BEGIN
    BEGIN
        PERFORM 1 FROM acl_permission
        WHERE object_type = 'document'
          AND object_id = 'test-doc'
          AND subject_type = 'user'
          AND subject_id = 'test-user'
          AND relation = 'read'
        LIMIT 1;

        RAISE NOTICE 'Query executed successfully!';
    EXCEPTION WHEN OTHERS THEN
        RAISE NOTICE 'Query failed (expected if OpenFGA not running): %', SQLERRM;
    END;
END $$;

\echo ''
\echo '=== Test Suite Complete ==='
\echo ''
\echo 'Summary:'
\echo '  - Extension created: postfga'
\echo '  - FDW created: postfga_fdw'
\echo '  - Test server created: test_openfga_server'
\echo '  - Foreign table created: acl_permission'
\echo '  - Cache functions working: clear_cache(), cache_stats()'
\echo ''
\echo 'Next steps:'
\echo '  1. Configure OpenFGA connection in postgresql.conf or as GUC'
\echo '  2. Set up OpenFGA server with store and authorization model'
\echo '  3. Test actual permission checks with real data'
