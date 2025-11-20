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
 * Request type enumeration
 */
typedef enum
{
    REQ_TYPE_CHECK       = 1,    /* Check authorization (read) */
    REQ_TYPE_WRITE       = 2,    /* Write tuple (grant permission) */
    REQ_TYPE_DELETE      = 3,    /* Delete tuple (revoke permission) */
    REQ_TYPE_EXPAND      = 4,    /* Expand relationship */
    REQ_TYPE_LIST_OBJECTS = 5,   /* List objects */
    REQ_TYPE_LIST_USERS   = 6    /* List users */
} RequestType;

/*
 * Base request structure (common fields for all request types)
 */
typedef struct BaseRequest
{
    uint32        request_id;
    RequestType   type;           /* Request type discriminator */
    RequestStatus status;

    /* Backend identification */
    pid_t         backend_pid;
    int32         backend_id;
    Latch        *backend_latch;  /* Latch to wake up backend */

    /* Common result fields */
    bool          success;
    uint32        error_code;

    /* Timestamps */
    TimestampTz   created_at;
    TimestampTz   completed_at;
} BaseRequest;

/*
 * Check request (authorization check)
 */
typedef struct CheckRequest
{
    BaseRequest   base;

    /* Check parameters */
    char          object_type[OBJECT_TYPE_MAX_LEN];
    char          object_id[OBJECT_ID_MAX_LEN];
    char          subject_type[SUBJECT_TYPE_MAX_LEN];
    char          subject_id[SUBJECT_ID_MAX_LEN];
    char          relation[RELATION_MAX_LEN];

    /* Check result */
    bool          allowed;
} CheckRequest;

/*
 * Write request (grant permission / add tuple)
 */
typedef struct WriteRequest
{
    BaseRequest   base;

    /* Tuple to write */
    char          object_type[OBJECT_TYPE_MAX_LEN];
    char          object_id[OBJECT_ID_MAX_LEN];
    char          subject_type[SUBJECT_TYPE_MAX_LEN];
    char          subject_id[SUBJECT_ID_MAX_LEN];
    char          relation[RELATION_MAX_LEN];
} WriteRequest;

/*
 * Delete request (revoke permission / delete tuple)
 */
typedef struct DeleteRequest
{
    BaseRequest   base;

    /* Tuple to delete */
    char          object_type[OBJECT_TYPE_MAX_LEN];
    char          object_id[OBJECT_ID_MAX_LEN];
    char          subject_type[SUBJECT_TYPE_MAX_LEN];
    char          subject_id[SUBJECT_ID_MAX_LEN];
    char          relation[RELATION_MAX_LEN];
} DeleteRequest;

/*
 * Request payload (tagged union)
 *
 * Use base.type to determine which struct to access
 */
typedef union RequestPayload
{
    BaseRequest    base;
    CheckRequest   check;
    WriteRequest   write;
    DeleteRequest  delete_req;
} RequestPayload;

/*
 * Legacy typedef for backward compatibility
 * TODO: Remove after migration
 */
typedef CheckRequest GrpcRequest;

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

/* Dequeue requests as RequestPayload (for BGW with multi-type support) */
bool dequeue_requests(RequestPayload *requests, uint32 *count);

/* Wait for gRPC result */
bool wait_for_grpc_result(uint32 request_id, bool *allowed, uint32 *error_code);

/* Set gRPC result */
void set_grpc_result(uint32 request_id, bool allowed, uint32 error_code);

/* Notify BGW of pending work */
void notify_bgw_of_pending_work(void);

/* Queue management functions */
uint32 clear_completed_requests(void);
bool get_queue_stats(uint32 *size, uint32 *capacity);

/* Write tuple request */
uint32 enqueue_write_request(const char *object_type, const char *object_id,
                             const char *subject_type, const char *subject_id,
                             const char *relation);

#endif /* POSTFGA_QUEUE_H */
