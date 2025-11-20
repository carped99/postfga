-- guc_test.sql
-- Test GUC variables for PostFGA extension

-- Load the extension (if not already loaded)
-- CREATE EXTENSION IF NOT EXISTS postfga_fdw;

-- Test 1: Check default values
SHOW openfga.endpoint;
SHOW openfga.store_id;
SHOW openfga.authorization_model_id;
SHOW openfga.relations;
SHOW openfga.cache_ttl_ms;
SHOW openfga.max_cache_entries;
SHOW openfga.bgw_workers;
SHOW openfga.fallback_to_grpc_on_miss;

-- Test 2: Set valid values
SET openfga.endpoint = 'dns:///test-server:9090';
SHOW openfga.endpoint;

SET openfga.store_id = 'test-store-123';
SHOW openfga.store_id;

SET openfga.authorization_model_id = 'model-456';
SHOW openfga.authorization_model_id;

SET openfga.relations = 'read,write,admin';
SHOW openfga.relations;

SET openfga.cache_ttl_ms = 30000;
SHOW openfga.cache_ttl_ms;

-- Test 3: Test boundary values
-- Min cache_ttl_ms (1000)
SET openfga.cache_ttl_ms = 1000;
SHOW openfga.cache_ttl_ms;

-- Max cache_ttl_ms (3600000)
SET openfga.cache_ttl_ms = 3600000;
SHOW openfga.cache_ttl_ms;

-- Test 4: Test boolean values
SET openfga.fallback_to_grpc_on_miss = true;
SHOW openfga.fallback_to_grpc_on_miss;

SET openfga.fallback_to_grpc_on_miss = false;
SHOW openfga.fallback_to_grpc_on_miss;

-- Test 5: Test invalid values (should fail)
-- This should fail (below min)
\set VERBOSITY verbose
\echo 'Testing invalid cache_ttl_ms (too low):'
DO $$
BEGIN
    EXECUTE 'SET openfga.cache_ttl_ms = 500';
    RAISE NOTICE 'ERROR: Should have failed!';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Expected error: %', SQLERRM;
END $$;

-- This should fail (above max)
\echo 'Testing invalid cache_ttl_ms (too high):'
DO $$
BEGIN
    EXECUTE 'SET openfga.cache_ttl_ms = 4000000';
    RAISE NOTICE 'ERROR: Should have failed!';
EXCEPTION WHEN OTHERS THEN
    RAISE NOTICE 'Expected error: %', SQLERRM;
END $$;

-- Test 6: Reset to defaults
RESET openfga.endpoint;
RESET openfga.store_id;
RESET openfga.authorization_model_id;
RESET openfga.relations;
RESET openfga.cache_ttl_ms;
RESET openfga.fallback_to_grpc_on_miss;

-- Verify reset
SHOW openfga.endpoint;
SHOW openfga.store_id;
SHOW openfga.relations;
SHOW openfga.cache_ttl_ms;
SHOW openfga.fallback_to_grpc_on_miss;

-- Test 7: Test with many relations (up to 64)
SET openfga.relations = 'r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15,r16,r17,r18,r19,r20';
SHOW openfga.relations;

-- Test 8: Test empty string values
SET openfga.store_id = '';
SHOW openfga.store_id;

SET openfga.authorization_model_id = '';
SHOW openfga.authorization_model_id;

\echo 'All GUC tests completed!'
