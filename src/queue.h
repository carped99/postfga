/*-------------------------------------------------------------------------
 *
 * queue.h
 *    Request queue operations for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTFGA_QUEUE_H
#define POSTFGA_QUEUE_H

#include <postgres.h>

#include "common.h"
#include "state.h"

typedef enum
{
    REQ_STATUS_PENDING   = 0,
    REQ_STATUS_COMPLETED = 1,
    REQ_STATUS_ERROR     = 2
} RequestStatus;

/*
 * gRPC Request Queue Entry (shared memory structure)
 */
typedef struct GrpcRequest
{
    uint32        request_id;
    RequestStatus status;

    /* Backend identification */
    pid_t         backend_pid;
    int32         backend_id;

    /* Permission check parameters */
    char          object_type[OBJECT_TYPE_MAX_LEN];
    char          object_id[OBJECT_ID_MAX_LEN];
    char          subject_type[SUBJECT_TYPE_MAX_LEN];
    char          subject_id[SUBJECT_ID_MAX_LEN];
    char          relation[RELATION_MAX_LEN];

    /* Result */
    bool          allowed;
    uint32        error_code;

    /* Timestamp */
    TimestampTz   created_at;
} GrpcRequest;

/* -------------------------------------------------------------------------
 * Request queue operations (stub for future use)
 * -------------------------------------------------------------------------
 */

/* Enqueue a gRPC request */
uint32 enqueue_grpc_request(const char *object_type, const char *object_id,
                           const char *subject_type, const char *subject_id,
                           const char *relation);

/* Dequeue gRPC requests for processing */
bool dequeue_grpc_requests(GrpcRequest *requests, uint32 *count);

/* Wait for gRPC result */
bool wait_for_grpc_result(uint32 request_id, bool *allowed, uint32 *error_code);

/* Set gRPC result */
void set_grpc_result(uint32 request_id, bool allowed, uint32 error_code);

/* Notify BGW of pending work */
void notify_bgw_of_pending_work(void);

/* Queue management functions */
uint32 clear_completed_requests(void);
bool get_queue_stats(uint32 *size, uint32 *capacity);

#endif /* POSTFGA_QUEUE_H */
