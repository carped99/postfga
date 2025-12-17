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
 * Static private helpers
 * -------------------------------------------------------------------------
 */
static const char* env_or_default(const char* env_name, const char* default_value)
{
    const char* val = getenv(env_name);
    return (val && val[0] != '\0') ? val : default_value;
}

static bool validate_endpoint(char** newval, void** extra, GucSource source)
{
    (void)extra;

    if (source == PGC_S_DEFAULT || source == PGC_S_DYNAMIC_DEFAULT)
        return true;

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

    if (source == PGC_S_DEFAULT || source == PGC_S_DYNAMIC_DEFAULT)
        return true;

    if (newval == NULL || *newval == NULL || (*newval)[0] == '\0')
    {
        GUC_check_errmsg("postfga.store_id must not be empty");
        GUC_check_errhint("Specifies the store ID to use in OpenFGA (can be set at system/db/role/session level).");
        GUC_check_errdetail("Set the store ID for OpenFGA.");
        return false;
    }

    return true;
}

static bool validate_cache_size(int* newval, void** extra, GucSource source)
{
    int v = *newval;

    (void)extra;

    if (source == PGC_S_DEFAULT || source == PGC_S_DYNAMIC_DEFAULT)
        return true;

    if (v < 1)
        v = 1;

    *newval = v;

    return true;
}

/*
 * fga_guc_init
 *
 * Define all GUC variables and bind them to the global config struct.
 * Must be called from _PG_init().
 */
void fga_guc_init(void)
{
    FgaConfig* cfg = fga_get_config();

    /* fga.endpoint */
    DefineCustomStringVariable("fga.endpoint",
                               "OpenFGA gRPC endpoint address",
                               "Specifies the gRPC endpoint for OpenFGA server (e.g., 'dns:///openfga:8081')",
                               &cfg->endpoint,
                               env_or_default("FGA_ENDPOINT", ""),
                               PGC_SIGHUP,
                               GUC_SUPERUSER_ONLY,
                               validate_endpoint,
                               NULL, /* assign_hook */
                               NULL  /* show_hook */
    );

    /* fga.store_id */
    DefineCustomStringVariable("fga.store_id",
                               "OpenFGA store ID",
                               "Specifies the store ID to use in OpenFGA (can be set at system/db/role/session level).",
                               &cfg->store_id,
                               env_or_default("FGA_STORE_ID", ""),
                               PGC_USERSET,
                               0,
                               validate_store_id,
                               NULL,
                               NULL);

    /* fga.model_id */
    DefineCustomStringVariable("fga.model_id",
                               "OpenFGA model ID (optional)",
                               "Specifies the model ID to use. If empty, uses the latest model.",
                               &cfg->model_id,
                               env_or_default("FGA_MODEL_ID", ""),
                               PGC_SUSET,
                               GUC_SUPERUSER_ONLY,
                               NULL,
                               NULL,
                               NULL);

    /* fga.cache_enabled */
    DefineCustomBoolVariable("fga.cache_enabled",
                             "Enable or disable the FGA permission cache",
                             "Specifies whether to enable or disable the permission cache",
                             &cfg->cache_enabled,
                             false,
                             PGC_SIGHUP,
                             0,
                             NULL,
                             NULL,
                             NULL);

    DefineCustomIntVariable("fga.cache_size",
                            "Size of FGA cache.",
                            NULL,
                            &cfg->cache_size,
                            32,
                            1,
                            1024,
                            PGC_POSTMASTER,
                            GUC_UNIT_MB,
                            validate_cache_size,
                            NULL,
                            NULL);

    /* fga.cache_ttl_ms */
    DefineCustomIntVariable("fga.cache_ttl_ms",
                            "Cache entry time-to-live in milliseconds",
                            "Specifies how long cache entries remain valid (in milliseconds)",
                            &cfg->cache_ttl_ms,
                            60000,
                            1000,    /* min: 1 second */
                            3600000, /* max: 1 hour */
                            PGC_SIGHUP, // BGW 시작 시 적용
                            GUC_UNIT_MS,
                            NULL,
                            NULL,
                            NULL);
}

void fga_guc_fini(void)
{
    // noop
}
