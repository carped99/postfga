-- postfga_fdw--1.0.0.sql

-- Create schema for the extension
-- CREATE SCHEMA IF NOT EXISTS postfga_fdw;

-- FDW handler function
CREATE FUNCTION postfga_fdw_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

-- FDW validator function
CREATE FUNCTION postfga_fdw_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

-- Create the FDW
CREATE FOREIGN DATA WRAPPER postfga_fdw
  HANDLER postfga_fdw_handler
  VALIDATOR postfga_fdw_validator;

-- Core functions for permission checking and tuple management
CREATE OR REPLACE FUNCTION postfga_check(
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

CREATE OR REPLACE FUNCTION postfga_write_tuple(
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

CREATE OR REPLACE FUNCTION postfga_delete_tuple(
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

CREATE OR REPLACE FUNCTION postfga_create_store(
    name text
)
RETURNS TABLE(id text, name text)
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT PARALLEL SAFE VOLATILE COST 10000;

CREATE OR REPLACE FUNCTION postfga_delete_store(
    id text
)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT PARALLEL SAFE VOLATILE COST 10000;

CREATE FUNCTION postfga_stats()
RETURNS TABLE (
    l1_hits           bigint,
    l1_misses         bigint,
    l1_hit_rate       float8,
    l2_misses         bigint,
    l2_hits           bigint,
    l2_hit_rate       float8,
    total_hit_rate    float8
)
AS 'MODULE_PATHNAME'
LANGUAGE C STABLE;


-- -- Grant usage to public (can be restricted later)
-- GRANT USAGE ON FOREIGN DATA WRAPPER postfga_fdw TO PUBLIC;

-- -- Comment on extension
-- COMMENT ON FOREIGN DATA WRAPPER postfga_fdw
--   IS 'Foreign data wrapper for PostFGA permission checking';
-- -- Utility functions for cache management (future extension)
-- -- These will be expanded in later versions
-- CREATE FUNCTION postfga_fdw.clear_cache()
--   RETURNS void
--   LANGUAGE C
--   AS 'MODULE_PATHNAME', 'postfga_fdw_clear_cache'
--   STRICT;

-- COMMENT ON FUNCTION postfga_fdw.clear_cache()
--   IS 'Clear the shared memory cache for ACL permissions';

-- CREATE FUNCTION postfga_fdw.cache_stats()
--   RETURNS TABLE (
--     total_entries bigint,
--     cached_entries bigint,
--     cache_hits bigint,
--     cache_misses bigint
--   )
--   LANGUAGE C
--   AS 'MODULE_PATHNAME', 'postfga_fdw_cache_stats'
--   STRICT;

-- COMMENT ON FUNCTION postfga_fdw.cache_stats()
--   IS 'Get statistics about the shared memory cache';
