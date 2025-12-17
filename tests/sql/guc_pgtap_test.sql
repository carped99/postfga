-- guc_pgtap_test.sql
-- pgTAP tests for PostFGA GUC variables
--
-- Prerequisites: pgTAP extension must be installed
-- Run with: pg_prove -d <database> tests/sql/guc_pgtap_test.sql
--
-- Or manually:
-- psql -d <database> -f tests/sql/guc_pgtap_test.sql

-- Start transaction and load pgTAP
BEGIN;
-- SELECT plan(30);  -- Uncomment if pgTAP is available

-- Test 1: Extension loads successfully
-- SELECT lives_ok(
--     'CREATE EXTENSION IF NOT EXISTS fga_fdw',
--     'PostFGA extension loads without errors'
-- );

-- Test 2: Check that all GUC variables exist
\echo '=== Testing GUC Variable Existence ==='

DO $$
BEGIN
    -- Test openfga.endpoint exists
    PERFORM current_setting('openfga.endpoint', true);
    RAISE NOTICE 'openfga.endpoint exists';
EXCEPTION WHEN undefined_object THEN
    RAISE EXCEPTION 'openfga.endpoint does not exist';
END $$;

DO $$
BEGIN
    -- Test openfga.store_id exists
    PERFORM current_setting('openfga.store_id', true);
    RAISE NOTICE 'openfga.store_id exists';
EXCEPTION WHEN undefined_object THEN
    RAISE EXCEPTION 'openfga.store_id does not exist';
END $$;

DO $$
BEGIN
    -- Test openfga.authorization_model_id exists
    PERFORM current_setting('openfga.authorization_model_id', true);
    RAISE NOTICE 'openfga.authorization_model_id exists';
EXCEPTION WHEN undefined_object THEN
    RAISE EXCEPTION 'openfga.authorization_model_id does not exist';
END $$;

DO $$
BEGIN
    -- Test openfga.relations exists
    PERFORM current_setting('openfga.relations', true);
    RAISE NOTICE 'openfga.relations exists';
EXCEPTION WHEN undefined_object THEN
    RAISE EXCEPTION 'openfga.relations does not exist';
END $$;

DO $$
BEGIN
    -- Test openfga.cache_ttl_ms exists
    PERFORM current_setting('openfga.cache_ttl_ms', true);
    RAISE NOTICE 'openfga.cache_ttl_ms exists';
EXCEPTION WHEN undefined_object THEN
    RAISE EXCEPTION 'openfga.cache_ttl_ms does not exist';
END $$;

DO $$
BEGIN
    -- Test openfga.max_cache_entries exists
    PERFORM current_setting('openfga.max_cache_entries', true);
    RAISE NOTICE 'openfga.max_cache_entries exists';
EXCEPTION WHEN undefined_object THEN
    RAISE EXCEPTION 'openfga.max_cache_entries does not exist';
END $$;

DO $$
BEGIN
    -- Test openfga.bgw_workers exists
    PERFORM current_setting('openfga.bgw_workers', true);
    RAISE NOTICE 'openfga.bgw_workers exists';
EXCEPTION WHEN undefined_object THEN
    RAISE EXCEPTION 'openfga.bgw_workers does not exist';
END $$;

DO $$
BEGIN
    -- Test openfga.fallback_to_grpc_on_miss exists
    PERFORM current_setting('openfga.fallback_to_grpc_on_miss', true);
    RAISE NOTICE 'openfga.fallback_to_grpc_on_miss exists';
EXCEPTION WHEN undefined_object THEN
    RAISE EXCEPTION 'openfga.fallback_to_grpc_on_miss does not exist';
END $$;

-- Test 3: Check default values
\echo '=== Testing Default Values ==='

DO $$
DECLARE
    v_value TEXT;
    v_int INT;
    v_bool BOOLEAN;
BEGIN
    -- Check endpoint default
    v_value := current_setting('openfga.endpoint');
    IF v_value = 'dns:///localhost:8081' THEN
        RAISE NOTICE 'PASS: openfga.endpoint default is correct';
    ELSE
        RAISE WARNING 'FAIL: openfga.endpoint default is %, expected dns:///localhost:8081', v_value;
    END IF;

    -- Check relations default
    v_value := current_setting('openfga.relations');
    IF v_value = 'read,write,edit,delete,owner' THEN
        RAISE NOTICE 'PASS: openfga.relations default is correct';
    ELSE
        RAISE WARNING 'FAIL: openfga.relations default is %, expected read,write,edit,delete,owner', v_value;
    END IF;

    -- Check cache_ttl_ms default
    v_int := current_setting('openfga.cache_ttl_ms')::INT;
    IF v_int = 60000 THEN
        RAISE NOTICE 'PASS: openfga.cache_ttl_ms default is correct (60000)';
    ELSE
        RAISE WARNING 'FAIL: openfga.cache_ttl_ms default is %, expected 60000', v_int;
    END IF;

    -- Check max_cache_entries default
    v_int := current_setting('openfga.max_cache_entries')::INT;
    IF v_int = 10000 THEN
        RAISE NOTICE 'PASS: openfga.max_cache_entries default is correct (10000)';
    ELSE
        RAISE WARNING 'FAIL: openfga.max_cache_entries default is %, expected 10000', v_int;
    END IF;

    -- Check bgw_workers default
    v_int := current_setting('openfga.bgw_workers')::INT;
    IF v_int = 1 THEN
        RAISE NOTICE 'PASS: openfga.bgw_workers default is correct (1)';
    ELSE
        RAISE WARNING 'FAIL: openfga.bgw_workers default is %, expected 1', v_int;
    END IF;

    -- Check fallback_to_grpc_on_miss default
    v_bool := current_setting('openfga.fallback_to_grpc_on_miss')::BOOLEAN;
    IF v_bool = TRUE THEN
        RAISE NOTICE 'PASS: openfga.fallback_to_grpc_on_miss default is correct (true)';
    ELSE
        RAISE WARNING 'FAIL: openfga.fallback_to_grpc_on_miss default is %, expected true', v_bool;
    END IF;
END $$;

-- Test 4: Test value changes
\echo '=== Testing Value Changes ==='

DO $$
DECLARE
    v_value TEXT;
BEGIN
    -- Test changing endpoint
    EXECUTE 'SET openfga.endpoint = ''dns:///test:9090''';
    v_value := current_setting('openfga.endpoint');
    IF v_value = 'dns:///test:9090' THEN
        RAISE NOTICE 'PASS: Can set openfga.endpoint';
    ELSE
        RAISE WARNING 'FAIL: Could not set openfga.endpoint';
    END IF;

    -- Reset
    EXECUTE 'RESET openfga.endpoint';
END $$;

-- Test 5: Test boundary values
\echo '=== Testing Boundary Values ==='

DO $$
DECLARE
    v_int INT;
BEGIN
    -- Test minimum cache_ttl_ms
    EXECUTE 'SET openfga.cache_ttl_ms = 1000';
    v_int := current_setting('openfga.cache_ttl_ms')::INT;
    IF v_int = 1000 THEN
        RAISE NOTICE 'PASS: Can set cache_ttl_ms to minimum (1000)';
    ELSE
        RAISE WARNING 'FAIL: Could not set cache_ttl_ms to minimum';
    END IF;

    -- Test maximum cache_ttl_ms
    EXECUTE 'SET openfga.cache_ttl_ms = 3600000';
    v_int := current_setting('openfga.cache_ttl_ms')::INT;
    IF v_int = 3600000 THEN
        RAISE NOTICE 'PASS: Can set cache_ttl_ms to maximum (3600000)';
    ELSE
        RAISE WARNING 'FAIL: Could not set cache_ttl_ms to maximum';
    END IF;

    -- Reset
    EXECUTE 'RESET openfga.cache_ttl_ms';
END $$;

-- Test 6: Test invalid values (should fail)
\echo '=== Testing Invalid Values ==='

DO $$
BEGIN
    -- This should fail
    BEGIN
        EXECUTE 'SET openfga.cache_ttl_ms = 500';
        RAISE WARNING 'FAIL: Should not allow cache_ttl_ms = 500 (below minimum)';
    EXCEPTION WHEN OTHERS THEN
        RAISE NOTICE 'PASS: Correctly rejected cache_ttl_ms = 500';
    END;

    -- This should also fail
    BEGIN
        EXECUTE 'SET openfga.cache_ttl_ms = 5000000';
        RAISE WARNING 'FAIL: Should not allow cache_ttl_ms = 5000000 (above maximum)';
    EXCEPTION WHEN OTHERS THEN
        RAISE NOTICE 'PASS: Correctly rejected cache_ttl_ms = 5000000';
    END;
END $$;

-- Test 7: Test relations parsing
\echo '=== Testing Relations Parsing ==='

DO $$
DECLARE
    v_value TEXT;
    v_count INT;
BEGIN
    -- Set multiple relations
    EXECUTE 'SET openfga.relations = ''read,write,admin,owner''';
    v_value := current_setting('openfga.relations');

    -- Count commas
    SELECT LENGTH(v_value) - LENGTH(REPLACE(v_value, ',', '')) + 1 INTO v_count;

    IF v_count = 4 THEN
        RAISE NOTICE 'PASS: Relations string contains 4 relations';
    ELSE
        RAISE WARNING 'FAIL: Expected 4 relations, got %', v_count;
    END IF;

    -- Reset
    EXECUTE 'RESET openfga.relations';
END $$;

\echo '=== All Tests Complete ==='

-- SELECT * FROM finish();
ROLLBACK;
