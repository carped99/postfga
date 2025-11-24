-- postfga_fdw--1.0.0.sql

-- Create schema for the extension
-- CREATE SCHEMA IF NOT EXISTS postfga_fdw;

-- -- FDW handler function
-- CREATE FUNCTION postfga_fdw_handler()
--   RETURNS fdw_handler
--   AS 'MODULE_PATHNAME', 'postfga_fdw_handler'
--   LANGUAGE C STRICT;

-- -- FDW validator function
-- CREATE FUNCTION postfga_fdw_validator(text[], oid)
--   RETURNS void
--   AS 'MODULE_PATHNAME', 'postfga_fdw_validator'
--   LANGUAGE C STRICT;

-- CREATE OR REPLACE FUNCTION postfga_write_tuple(
--     object_type text,
--     object_id text,
--     subject_type text,
--     subject_id text,
--     relation text
-- )
-- RETURNS boolean
-- AS 'MODULE_PATHNAME', 'postfga_write_tuple'
-- LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION postfga_check(
    object_type text,
    object_id text,
    subject_type text,
    subject_id text,
    relation text
)
RETURNS boolean
AS 'MODULE_PATHNAME'
LANGUAGE C 
STRICT 
PARALLEL SAFE;  

-- Create the FDW
-- CREATE FOREIGN DATA WRAPPER postfga_fdw
--   HANDLER postfga_fdw_handler
--   VALIDATOR postfga_fdw_validator;

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
