-- postfga--1.0.0.sql

-- Create schema for the extension
-- CREATE SCHEMA IF NOT EXISTS fga_fdw;

-- FDW handler function
CREATE FUNCTION fga_fdw_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

-- FDW validator function
CREATE FUNCTION fga_fdw_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

-- Create the FDW
CREATE FOREIGN DATA WRAPPER fga_fdw
  HANDLER fga_fdw_handler
  VALIDATOR fga_fdw_validator;

-- Core functions for permission checking and tuple management
CREATE OR REPLACE FUNCTION fga_check(
    object_type text,
    object_id text,
    subject_type text,
    subject_id text,
    relation text,
    options jsonb DEFAULT NULL
)
RETURNS boolean
AS 'MODULE_PATHNAME'
LANGUAGE C PARALLEL SAFE VOLATILE COST 10000;

CREATE OR REPLACE FUNCTION fga_write_tuple(
    object_type text,
    object_id text,
    subject_type text,
    subject_id text,
    relation text,
    options jsonb DEFAULT NULL
)
RETURNS boolean
AS 'MODULE_PATHNAME'
LANGUAGE C PARALLEL SAFE VOLATILE COST 10000;

CREATE OR REPLACE FUNCTION fga_delete_tuple(
    object_type text,
    object_id text,
    subject_type text,
    subject_id text,
    relation text,
    options jsonb DEFAULT NULL
)
RETURNS boolean
AS 'MODULE_PATHNAME'
LANGUAGE C PARALLEL SAFE VOLATILE COST 10000;

CREATE OR REPLACE FUNCTION fga_create_store(
    name text
)
RETURNS TABLE(id text, name text)
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT PARALLEL SAFE VOLATILE COST 10000;

CREATE OR REPLACE FUNCTION fga_delete_store(
    id text
)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT PARALLEL SAFE VOLATILE COST 10000;

CREATE OR REPLACE FUNCTION fga_stats()
RETURNS TABLE (
    section text,
    metric text,
    value bigint
)
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE;


-- -- Grant usage to public (can be restricted later)
-- GRANT USAGE ON FOREIGN DATA WRAPPER fga_fdw TO PUBLIC;

-- -- Comment on extension
-- COMMENT ON FOREIGN DATA WRAPPER fga_fdw
--   IS 'Foreign data wrapper for PostFGA permission checking';
-- -- Utility functions for cache management (future extension)
-- -- These will be expanded in later versions
-- CREATE FUNCTION fga_fdw.clear_cache()
--   RETURNS void
--   LANGUAGE C
--   AS 'MODULE_PATHNAME', 'fga_fdw_clear_cache'
--   STRICT;

-- COMMENT ON FUNCTION fga_fdw.clear_cache()
--   IS 'Clear the shared memory cache for ACL permissions';

-- CREATE FUNCTION fga_fdw.cache_stats()
--   RETURNS TABLE (
--     total_entries bigint,
--     cached_entries bigint,
--     cache_hits bigint,
--     cache_misses bigint
--   )
--   LANGUAGE C
--   AS 'MODULE_PATHNAME', 'fga_fdw_cache_stats'
--   STRICT;

-- COMMENT ON FUNCTION fga_fdw.cache_stats()
--   IS 'Get statistics about the shared memory cache';
