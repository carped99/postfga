-- Initialize openfga_fdw extension on startup

-- Set GUC parameters
SET openfga_fdw.endpoint = 'dns:///openfga:8081';
SET openfga_fdw.store_id = 'test-store';
SET openfga_fdw.relations = 'read,write,edit,delete,download,owner';
SET openfga_fdw.cache_ttl_ms = 60000;

-- Create extension
CREATE EXTENSION IF NOT EXISTS openfga_fdw;

-- Create test server
CREATE SERVER IF NOT EXISTS openfga_server
  FOREIGN DATA WRAPPER openfga_fdw
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
