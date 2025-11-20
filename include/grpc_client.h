/*
 * grpc_client.h
 *
 * OpenFGA gRPC async client using standalone ASIO
 */

#ifndef POSTFGA_GRPC_CLIENT_H
#define POSTFGA_GRPC_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Opaque handle for gRPC client */
typedef struct GrpcClient GrpcClient;

/* Check request parameters for gRPC API */
typedef struct {
    const char *store_id;
    const char *authorization_model_id;  /* Optional, can be NULL */

    /* Tuple key */
    const char *object_type;
    const char *object_id;
    const char *relation;
    const char *subject_type;
    const char *subject_id;
} GrpcCheckRequest;

/* Check response */
typedef struct {
    bool allowed;
    uint32_t error_code;
    char error_message[256];
} CheckResponse;

/* Callback for async check completion */
typedef void (*CheckCallback)(const CheckResponse *response, void *user_data);

/*
 * Initialize gRPC client
 *
 * endpoint: gRPC endpoint (e.g., "dns:///localhost:8081")
 * Returns: Pointer to initialized client, or NULL on error
 */
GrpcClient *grpc_client_init(const char *endpoint);

/*
 * Shutdown gRPC client and free resources
 */
void grpc_client_shutdown(GrpcClient *client);

/*
 * Perform synchronous Check API call
 *
 * Returns: true if successful, false on error
 */
bool grpc_client_check_sync(GrpcClient *client,
                            const GrpcCheckRequest *request,
                            CheckResponse *response);

/*
 * Perform asynchronous Check API call
 *
 * callback: Function to call when request completes
 * user_data: User data passed to callback
 * Returns: true if request queued successfully, false on error
 */
bool grpc_client_check_async(GrpcClient *client,
                             const GrpcCheckRequest *request,
                             CheckCallback callback,
                             void *user_data);

/*
 * Process pending async operations (call from event loop)
 *
 * timeout_ms: Maximum time to block (0 = non-blocking)
 * Returns: Number of operations processed
 */
int grpc_client_poll(GrpcClient *client, int timeout_ms);

/*
 * Check if client is connected and healthy
 */
bool grpc_client_is_healthy(GrpcClient *client);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_GRPC_CLIENT_H */
