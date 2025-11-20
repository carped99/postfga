/*
 * test_grpc_client.c
 *
 * Test program for gRPC client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "grpc_client.h"

static volatile int async_completed = 0;

void async_check_callback(const CheckResponse *resp, void *user_data)
{
    const char *request_id = (const char *)user_data;

    printf("\n[ASYNC] Callback for request: %s\n", request_id);

    if (resp->error_code == 0) {
        printf("[ASYNC] Result: %s\n", resp->allowed ? "ALLOWED" : "DENIED");
    } else {
        printf("[ASYNC] Error %u: %s\n", resp->error_code, resp->error_message);
    }

    async_completed = 1;
}

int main(int argc, char **argv)
{
    const char *endpoint = "dns:///localhost:8081";
    const char *store_id = "01HXXX";  // Replace with your store ID

    if (argc > 1) {
        endpoint = argv[1];
    }
    if (argc > 2) {
        store_id = argv[2];
    }

    printf("=== OpenFGA gRPC Client Test ===\n\n");
    printf("Endpoint: %s\n", endpoint);
    printf("Store ID: %s\n\n", store_id);

    /* Initialize client */
    printf("[1] Initializing gRPC client...\n");
    GrpcClient *client = grpc_client_init(endpoint);
    if (!client) {
        fprintf(stderr, "ERROR: Failed to initialize gRPC client\n");
        return 1;
    }
    printf("    ✓ Client initialized\n\n");

    /* Check health */
    printf("[2] Checking client health...\n");
    if (grpc_client_is_healthy(client)) {
        printf("    ✓ Client is healthy\n\n");
    } else {
        printf("    ⚠ Client may not be connected\n\n");
    }

    /* Test 1: Synchronous Check */
    printf("[3] Testing synchronous Check...\n");

    CheckRequest sync_req = {
        .store_id = store_id,
        .authorization_model_id = NULL,
        .object_type = "document",
        .object_id = "test-doc-123",
        .relation = "read",
        .subject_type = "user",
        .subject_id = "alice"
    };

    CheckResponse sync_resp;

    printf("    Checking: user:alice can read document:test-doc-123\n");

    if (grpc_client_check_sync(client, &sync_req, &sync_resp)) {
        printf("    ✓ Check completed\n");
        printf("    Result: %s\n", sync_resp.allowed ? "ALLOWED" : "DENIED");
    } else {
        printf("    ✗ Check failed\n");
        printf("    Error %u: %s\n", sync_resp.error_code, sync_resp.error_message);
    }
    printf("\n");

    /* Test 2: Asynchronous Check */
    printf("[4] Testing asynchronous Check...\n");

    CheckRequest async_req = {
        .store_id = store_id,
        .authorization_model_id = NULL,
        .object_type = "document",
        .object_id = "test-doc-456",
        .relation = "write",
        .subject_type = "user",
        .subject_id = "bob"
    };

    printf("    Checking: user:bob can write document:test-doc-456\n");

    const char *request_id = "async-test-001";
    async_completed = 0;

    if (grpc_client_check_async(client, &async_req, async_check_callback, (void *)request_id)) {
        printf("    ✓ Async check queued\n");

        // Wait for completion (up to 10 seconds)
        int timeout = 100;  // 10 seconds
        while (!async_completed && timeout-- > 0) {
            usleep(100000);  // 100ms
        }

        if (async_completed) {
            printf("    ✓ Async check completed\n");
        } else {
            printf("    ⚠ Async check timed out\n");
        }
    } else {
        printf("    ✗ Failed to queue async check\n");
    }
    printf("\n");

    /* Test 3: Multiple rapid checks */
    printf("[5] Testing multiple rapid checks...\n");

    const char *users[] = {"alice", "bob", "charlie", "diana"};
    const char *relations[] = {"read", "write", "delete", "owner"};

    int success_count = 0;
    int total_count = 16;  // 4 users × 4 relations

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            CheckRequest req = {
                .store_id = store_id,
                .authorization_model_id = NULL,
                .object_type = "document",
                .object_id = "batch-test",
                .relation = relations[j],
                .subject_type = "user",
                .subject_id = users[i]
            };

            CheckResponse resp;

            if (grpc_client_check_sync(client, &req, &resp)) {
                success_count++;
            }
        }
    }

    printf("    Completed %d/%d checks\n", success_count, total_count);
    printf("\n");

    /* Shutdown */
    printf("[6] Shutting down client...\n");
    grpc_client_shutdown(client);
    printf("    ✓ Client shutdown complete\n\n");

    printf("=== Test Complete ===\n");

    return 0;
}
