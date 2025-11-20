#ifndef POSTFGA_GUC_H
#define POSTFGA_GUC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <postgres.h>

/* GUC Parameter Structure */
typedef struct {
    char *endpoint;                       /* OpenFGA gRPC endpoint */
    char *store_id;                       /* OpenFGA store ID */
    char *authorization_model_id;         /* Optional authorization model ID */
    char *relations;                      /* Comma-separated relation list */
    int cache_ttl_ms;                    /* Cache TTL in milliseconds */
    int max_cache_entries;                /* Maximum cache entries */
    int bgw_workers;                      /* Number of background workers */
    bool fallback_to_grpc_on_miss;        /* Fall back to gRPC on cache miss */
} PostfgaGuc;

/* GUC functions */
void postfga_guc_init(void);
void postfga_guc_fini(void);
void validate_guc_values(void);

PostfgaGuc *postfga_get_guc(void);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_GUC_H */