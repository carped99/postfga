#ifndef POSTFGA_CONFIG_H
#define POSTFGA_CONFIG_H

/*
 * Configuration structure for PostFGA extension
 */
typedef struct PostfgaConfig {
    char *endpoint;                   /* gRPC endpoint */
    char *store_id;                   /* Store ID */
    char *authorization_model_id;     /* Authorization Model ID */
    int cache_ttl_ms;              /* Cache TTL in milliseconds */
    int max_cache_entries;                /* Maximum cache entries */
    int bgw_workers;                      /* Number of background workers */
    bool fallback_to_grpc_on_miss;        /* Fall back to gRPC on cache miss */
} PostfgaConfig;

#endif /* POSTFGA_CONFIG_H */