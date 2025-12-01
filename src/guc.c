/*-------------------------------------------------------------------------
 *
 * guc.c
 *    GUC (Grand Unified Configuration) variable management for PostFGA.
 *
 * This module implements:
 *   - GUC variable definitions and initialization
 *   - Configuration parameter management
 *   - Parameter validation
 *
 *-------------------------------------------------------------------------
 */

#include "guc.h"
#include <postgres.h>

#include <string.h>
#include <utils/guc.h>

#include "config.h"
#include "relation.h"

/* -------------------------------------------------------------------------
 * Module-level state
 * -------------------------------------------------------------------------
 */

// GUC names
#define POSTFGA_GUC_ENDPOINT "postfga.endpoint"
#define POSTFGA_GUC_STORE_ID "postfga.store_id"
#define POSTFGA_GUC_AUTH_MODEL_ID "postfga.authorization_model_id"
#define POSTFGA_GUC_RELATIONS "postfga.relations"
#define POSTFGA_GUC_CACHE_TTL_MS "postfga.cache_ttl_ms"
#define POSTFGA_GUC_MAX_CACHE_ENTRIES "postfga.max_cache_entries"
#define POSTFGA_GUC_BGW_WORKERS "postfga.bgw_workers"
#define POSTFGA_GUC_FALLBACK_ON_MISS "postfga.fallback_to_grpc_on_miss"

// Default values
#define DEFAULT_ENDPOINT "dns:///localhost:8081"
#define DEFAULT_STORE_ID "changeme"
#define DEFAULT_AUTH_MODEL_ID ""
#define DEFAULT_RELATIONS "read,write,edit,delete,owner"
#define DEFAULT_CACHE_TTL_MS 60000
#define DEFAULT_MAX_CACHE_ENTRIES 10000
#define DEFAULT_BGW_WORKERS 1
#define DEFAULT_FALLBACK_TO_GRPC true
#define DEFAULT_CACHE_ENABLED true

/* -------------------------------------------------------------------------
 * Static private helpers
 * -------------------------------------------------------------------------
 */
static void postfga_guc_cache_init(void);

static const char* env_or_default(const char* env_name, const char* default_value)
{
    const char* val = getenv(env_name);
    return (val && val[0] != '\0') ? val : default_value;
}

static bool validate_endpoint(char** newval, void** extra, GucSource source)
{
    (void)extra;
    (void)source;

    if (newval == NULL || *newval == NULL || (*newval)[0] == '\0')
    {
        GUC_check_errmsg("postfga.endpoint must not be empty");
        GUC_check_errdetail("Set the gRPC endpoint address for OpenFGA server.");
    }

    return true;
}

static bool validate_store_id(char** newval, void** extra, GucSource source)
{
    (void)extra;
    (void)source;

    if (newval == NULL || *newval == NULL || (*newval)[0] == '\0')
    {
        GUC_check_errmsg("postfga.store_id must not be empty");
        GUC_check_errdetail("Set the store ID for OpenFGA.");
        return false;
    }

    return true;
}

/*
 * postfga_guc_init
 *
 * Define all GUC variables and bind them to the global config struct.
 * Must be called from _PG_init().
 */
void postfga_guc_init(void)
{
    PostfgaConfig* cfg = postfga_get_config();

    /* postfga.endpoint */
    DefineCustomStringVariable("postfga.endpoint",
                               "OpenFGA gRPC endpoint address",
                               "Specifies the gRPC endpoint for OpenFGA server (e.g., 'dns:///openfga:8081')",
                               &cfg->endpoint,
                               env_or_default("POSTFGA_ENDPOINT", DEFAULT_ENDPOINT),
                               PGC_SUSET,
                               GUC_SUPERUSER_ONLY,
                               validate_endpoint,
                               NULL, /* assign_hook */
                               NULL  /* show_hook */
    );

    /* postfga.store_id */
    DefineCustomStringVariable("postfga.store_id",
                               "OpenFGA store ID",
                               "Specifies the store ID to use in OpenFGA",
                               &cfg->store_id,
                               env_or_default("POSTFGA_STORE_ID", DEFAULT_STORE_ID),
                               PGC_SUSET,
                               GUC_SUPERUSER_ONLY,
                               validate_store_id,
                               NULL,
                               NULL);

    /* postfga.authorization_model_id */
    DefineCustomStringVariable("postfga.authorization_model_id",
                               "OpenFGA authorization model ID (optional)",
                               "Specifies the authorization model ID to use. If empty, uses the latest model.",
                               &cfg->authorization_model_id,
                               env_or_default("POSTFGA_AUTH_MODEL_ID", DEFAULT_AUTH_MODEL_ID),
                               PGC_SUSET,
                               GUC_SUPERUSER_ONLY,
                               NULL,
                               NULL,
                               NULL);

    postfga_guc_cache_init();

    /* openfga.bgw_workers */
    DefineCustomIntVariable("openfga.bgw_workers",
                            "Number of background worker processes",
                            "Specifies how many background workers to start for OpenFGA change stream processing",
                            &cfg->bgw_workers,
                            DEFAULT_BGW_WORKERS,
                            0,              /* min: 0 = disabled */
                            10,             /* max: 10 workers */
                            PGC_POSTMASTER, /* requires restart */
                            0,
                            NULL,
                            NULL,
                            NULL);

    DefineCustomIntVariable("postfga.max_slots",
                            "Maximum number of FGA slots in shared memory.",
                            NULL,
                            &cfg->max_slots,
                            1024 * 3,
                            16,
                            65535,
                            PGC_POSTMASTER,
                            0,
                            NULL,
                            NULL,
                            NULL);

    elog(LOG, "PostFGA: GUC variables initialized");
}

void postfga_guc_fini(void)
{
    // noop
}

static void postfga_guc_cache_init(void)
{
    PostfgaConfig* cfg = postfga_get_config();

    /* postfga.cache_enabled */
    DefineCustomBoolVariable("postfga.cache_enabled",
                             "Enable or disable the PostFGA permission cache",
                             "Specifies whether to enable or disable the permission cache",
                             &cfg->cache_enabled,
                             DEFAULT_CACHE_ENABLED,
                             PGC_POSTMASTER,
                             0,
                             NULL,
                             NULL,
                             NULL);

    DefineCustomIntVariable("postfga.cache_size",
                            "Number of entries in PostFGA shared L2 cache.",
                            NULL,
                            &cfg->cache_size,
                            16384,
                            1024,
                            10000000,
                            PGC_POSTMASTER,
                            0,
                            NULL,
                            NULL,
                            NULL);

    /* postfga.cache_ttl_ms */
    DefineCustomIntVariable("postfga.cache_ttl_ms",
                            "Cache entry time-to-live in milliseconds",
                            "Specifies how long cache entries remain valid (in milliseconds)",
                            &cfg->cache_ttl_ms,
                            DEFAULT_CACHE_TTL_MS,
                            1000,    /* min: 1 second */
                            3600000, /* max: 1 hour */
                            PGC_SUSET,
                            0,
                            NULL,
                            NULL,
                            NULL);

    /* openfga.max_cache_entries */
    DefineCustomIntVariable("openfga.max_cache_entries",
                            "Maximum number of cache entries",
                            "Specifies the maximum number of ACL permission entries to cache in shared memory",
                            &cfg->max_cache_entries,
                            DEFAULT_MAX_CACHE_ENTRIES,
                            100,            /* min */
                            1000000,        /* max: 1 million */
                            PGC_POSTMASTER, /* requires restart */
                            0,
                            NULL,
                            NULL,
                            NULL);
}

/*
 * validate_guc_values
 *
 * Validate GUC configuration values.
 * Should be called after GUCs are loaded (e.g., in shared memory startup).
 *
 * Logs warnings for invalid configurations.
 */
void validate_guc_values(void)
{
    PostfgaConfig* cfg = postfga_get_config();

    /* Validate endpoint */
    if (!cfg->endpoint || cfg->endpoint[0] == '\0')
    {
        elog(WARNING, "PostFGA: openfga_fdw.endpoint is not set");
    }

    /* Validate store_id */
    if (!cfg->store_id || cfg->store_id[0] == '\0')
    {
        elog(WARNING, "PostFGA: openfga_fdw.store_id is not set");
    }

    /* Validate cache_ttl_ms */
    if (cfg->cache_ttl_ms < 1000)
    {
        elog(WARNING,
             "PostFGA: openfga_fdw.cache_ttl_ms is very low (%d ms), "
             "this may cause excessive cache invalidation",
             cfg->cache_ttl_ms);
    }

    /* Validate max_cache_entries */
    if (cfg->max_cache_entries < 100)
    {
        elog(WARNING,
             "PostFGA: openfga_fdw.max_cache_entries is very low (%d), "
             "this may cause frequent cache evictions",
             cfg->max_cache_entries);
    }

    /* Validate bgw_workers */
    if (cfg->bgw_workers == 0)
    {
        elog(WARNING, "PostFGA: openfga_fdw.bgw_workers is 0, background worker will not start");
    }
    else if (cfg->bgw_workers > 1)
    {
        elog(WARNING,
             "PostFGA: openfga_fdw.bgw_workers is %d, but only 1 worker is currently supported",
             cfg->bgw_workers);
    }

    elog(DEBUG1, "PostFGA: GUC validation complete");
    elog(DEBUG1, "  endpoint: %s", cfg->endpoint ? cfg->endpoint : "(null)");
    elog(DEBUG1, "  store_id: %s", cfg->store_id ? cfg->store_id : "(null)");
    elog(DEBUG1,
         "  authorization_model_id: %s",
         cfg->authorization_model_id && cfg->authorization_model_id[0] ? cfg->authorization_model_id : "(empty)");
    elog(DEBUG1, "  cache_ttl_ms: %d", cfg->cache_ttl_ms);
    elog(DEBUG1, "  max_cache_entries: %d", cfg->max_cache_entries);
    elog(DEBUG1, "  bgw_workers: %d", cfg->bgw_workers);
    elog(DEBUG1, "  fallback_to_grpc_on_miss: %s", cfg->fallback_to_grpc_on_miss ? "true" : "false");
}
