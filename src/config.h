#ifndef POSTFGA_CONFIG_H
#define POSTFGA_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
/*
 * Configuration structure for PostFGA extension
 */
typedef struct PostfgaConfig
{
    char *endpoint;                /* gRPC endpoint */
    char *store_id;                /* Store ID */
    char *authorization_model_id;  /* Authorization Model ID */
    int cache_ttl_ms;              /* Cache TTL in milliseconds */
    int max_cache_entries;         /* Maximum cache entries */
    int bgw_workers;               /* Number of background workers */
    bool fallback_to_grpc_on_miss; /* Fall back to gRPC on cache miss */
    bool cache_enabled;            /* Enable or disable the permission cache */
    int cache_size;                /* Number of entries in shared L2 cache */
    int max_slots;                 /* Maximum number of request slots */
    int max_relations;             /* Maximum number of relations */
} PostfgaConfig;

#ifdef __cplusplus
extern "C"
{
#endif

    PostfgaConfig *postfga_get_config(void);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CONFIG_H */