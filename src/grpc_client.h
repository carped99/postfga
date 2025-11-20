#ifndef GRPC_CLIENT_H
#define GRPC_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <postgres.h>

/* OpenFGA gRPC Client */
typedef struct OpenFGAClient OpenFGAClient;

/* Result types for gRPC operations */
typedef enum {
    GRPC_RESULT_ALLOW = 1,
    GRPC_RESULT_DENY = 0,
    GRPC_RESULT_ERROR = -1
} GrpcCheckResult;

/* gRPC Client functions */
OpenFGAClient *grpc_client_create(const char *endpoint);
void grpc_client_destroy(OpenFGAClient *client);

GrpcCheckResult grpc_check_permission(
    OpenFGAClient *client,
    const char *object_type,
    const char *object_id,
    const char *relation,
    const char *subject_type,
    const char *subject_id);

bool grpc_read_changes(
    OpenFGAClient *client,
    char **out_object_type,
    char **out_object_id,
    char **out_subject_type,
    char **out_subject_id,
    char **out_relation);

#ifdef __cplusplus
}
#endif

#endif /* GRPC_CLIENT_H */
