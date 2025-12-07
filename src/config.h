#ifndef POSTFGA_CONFIG_H
#define POSTFGA_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
/*
 * Configuration structure for PostFGA extension
 */
typedef struct PostfgaConfig
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
} PostfgaConfig;

/* Global configuration instance */
extern PostfgaConfig postfga_config_instance_;

#ifdef __cplusplus
extern "C"
{
#endif

    static inline PostfgaConfig* postfga_get_config(void)
    {
        return &postfga_config_instance_;
    }

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CONFIG_H */
