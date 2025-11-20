/*
 * test_client.c
 *
 * Test program for client.cpp - enqueues test requests every second
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include "client.h"

static volatile sig_atomic_t running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

static void test_callback(const CheckResponse *response, void *user_data)
{
    int *request_num = (int *)user_data;

    printf("[Callback #%d] allowed=%d, error_code=%u, error_message='%s'\n",
           *request_num,
           response->allowed,
           response->error_code,
           response->error_message);

    free(request_num);
}

int main(int argc, char *argv[])
{
    const char *endpoint = "localhost:8080";
    int count = 0;

    if (argc > 1) {
        endpoint = argv[1];
    }

    printf("PostFGA Client Test\n");
    printf("===================\n");
    printf("Endpoint: %s\n", endpoint);
    printf("Press Ctrl+C to stop\n\n");

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize client */
    GrpcClient *client = postfga_client_init(endpoint);
    if (!client) {
        fprintf(stderr, "Failed to initialize gRPC client\n");
        return 1;
    }

    printf("Client initialized successfully\n");

    /* Check if client is healthy */
    if (postfga_client_is_healthy(client)) {
        printf("Client is healthy\n\n");
    } else {
        printf("Warning: Client is not healthy (may need OpenFGA server running)\n\n");
    }

    /* Main loop - send request every second */
    while (running) {
        count++;

        GrpcCheckRequest request = {
            .store_id = "01JD5G9VZN0MMXPVQR8NMKGDZ0",
            .authorization_model_id = NULL,
            .object_type = "document",
            .object_id = "budget",
            .relation = "reader",
            .subject_type = "user",
            .subject_id = "alice"
        };

        /* Allocate request number for callback */
        int *request_num = malloc(sizeof(int));
        *request_num = count;

        printf("[Request #%d] Sending async check: %s:%s#%s@%s:%s\n",
               count,
               request.object_type,
               request.object_id,
               request.relation,
               request.subject_type,
               request.subject_id);

        /* Send async request */
        bool success = postfga_client_check_async(client, &request, test_callback, request_num);

        if (!success) {
            fprintf(stderr, "[Request #%d] Failed to send async request\n", count);
            free(request_num);
        }

        /* Also test sync request every 5 requests */
        if (count % 5 == 0) {
            CheckResponse sync_response;

            printf("[Request #%d-sync] Sending sync check...\n", count);

            bool sync_success = postfga_client_check_sync(client, &request, &sync_response);

            if (sync_success) {
                printf("[Request #%d-sync] Result: allowed=%d\n",
                       count, sync_response.allowed);
            } else {
                printf("[Request #%d-sync] Error: %u - %s\n",
                       count,
                       sync_response.error_code,
                       sync_response.error_message);
            }
        }

        /* Wait 1 second */
        sleep(1);
    }

    printf("\n\nShutting down...\n");

    /* Cleanup */
    postfga_client_fini(client);

    printf("Test completed. Total requests sent: %d\n", count);

    return 0;
}
