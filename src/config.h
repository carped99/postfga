#ifndef FGA_CONFIG_H
#define FGA_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
/*
 * Configuration structure for PostFGA extension
 */
typedef struct FgaConfig
{
    char* endpoint;                /* gRPC endpoint */
    char* store_id;                /* Store ID */
    char* model_id;                /* Authorization Model ID */
    bool fallback_to_grpc_on_miss; /* Fall back to gRPC on cache miss */
    bool cache_enabled;            /* Enable or disable the permission cache */
    int cache_size;                /* Size in MB */
    int cache_ttl_ms;              /* Cache TTL in milliseconds */
    int max_slots;                 /* Maximum number of request slots */
    int max_relations;             /* Maximum number of relations */
} FgaConfig;

/* Global configuration instance */
extern FgaConfig fga_config_instance_;

#ifdef __cplusplus
extern "C"
{
#endif

    static inline FgaConfig* fga_get_config(void)
    {
        return &fga_config_instance_;
    }

#ifdef __cplusplus
}
#endif

#endif /* FGA_CONFIG_H */
