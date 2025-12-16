-- Initialize postfga extension on startup
DO $$
DECLARE
cur text := coalesce(current_setting('shared_preload_libraries', true), '');
  new text;
BEGIN
  IF cur ~ '(^|,)\s*postfga\s*(,|$)' THEN
    RETURN; -- 이미 있음
END IF;

  new := case
    when btrim(cur) = '' then 'postfga'
    else cur || ',postfga'
end;

EXECUTE format('ALTER SYSTEM SET shared_preload_libraries = %L', new);
END$$;

-- Set GUC parameters
SET postfga.endpoint = 'dns:///openfga:8081';
SET postfga.store_id = 'test-store';
SET postfga.cache_ttl_ms = 60000;

-- Create extension
CREATE EXTENSION IF NOT EXISTS postfga;

-- Create test server
CREATE SERVER IF NOT EXISTS postfga_server
  FOREIGN DATA WRAPPER postfga_fdw
  OPTIONS (
    endpoint 'dns:///openfga:8081',
    store_id 'test-store'
  );

-- Create foreign table for testing
CREATE FOREIGN TABLE IF NOT EXISTS acl_permission (
    object_type text,
    object_id text,
    subject_type text,
    subject_id text,
    relation text
) SERVER openfga_server;

-- Grant permissions
GRANT SELECT ON acl_permission TO PUBLIC;
