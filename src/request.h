/*-------------------------------------------------------------------------
 *
 * request.h
 *    Request operations for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTFGA_REQUEST_H
#define POSTFGA_REQUEST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>
#include <datatype/timestamp.h>
#include <storage/latch.h>

#include "postfga.h"

#ifdef __cplusplus
}
#endif

typedef enum
{
    REQ_STATUS_PENDING = 0,
    REQ_STATUS_COMPLETED = 1,
    REQ_STATUS_ERROR = 2
} RequestStatus;

/*
 * Request type enumeration
 */
typedef enum
{
    REQ_TYPE_CHECK = 1,        /* Check authorization (read) */
    REQ_TYPE_WRITE = 2,        /* Write tuple (grant permission) */
    REQ_TYPE_DELETE = 3,       /* Delete tuple (revoke permission) */
    REQ_TYPE_EXPAND = 4,       /* Expand relationship */
    REQ_TYPE_LIST_OBJECTS = 5, /* List objects */
    REQ_TYPE_LIST_USERS = 6    /* List users */
} RequestType;

/*
 * Base request structure (common fields for all request types)
 */
typedef struct BaseRequest
{
    uint32 request_id;
    RequestType type; /* Request type discriminator */
    RequestStatus status;

    /* Backend identification */
    pid_t backend_pid;
    int32 backend_id;
    Latch *backend_latch; /* Latch to wake up backend */

    /* Common result fields */
    bool success;
    uint32 error_code;

    /* Timestamps */
    TimestampTz created_at;
    TimestampTz completed_at;
} BaseRequest;

/*
 * Check request (authorization check)
 */
typedef struct CheckRequest
{
    BaseRequest base;

    /* Check parameters */
    char object_type[OBJECT_TYPE_MAX_LEN];
    char object_id[OBJECT_ID_MAX_LEN];
    char subject_type[SUBJECT_TYPE_MAX_LEN];
    char subject_id[SUBJECT_ID_MAX_LEN];
    char relation[RELATION_MAX_LEN];

    /* Check result */
    bool allowed;
} CheckRequest;

/*
 * Write request (grant permission / add tuple)
 */
typedef struct WriteRequest
{
    BaseRequest base;

    /* Tuple to write */
    char object_type[OBJECT_TYPE_MAX_LEN];
    char object_id[OBJECT_ID_MAX_LEN];
    char subject_type[SUBJECT_TYPE_MAX_LEN];
    char subject_id[SUBJECT_ID_MAX_LEN];
    char relation[RELATION_MAX_LEN];
} WriteRequest;

/*
 * Delete request (revoke permission / delete tuple)
 */
typedef struct DeleteRequest
{
    BaseRequest base;

    /* Tuple to delete */
    char object_type[OBJECT_TYPE_MAX_LEN];
    char object_id[OBJECT_ID_MAX_LEN];
    char subject_type[SUBJECT_TYPE_MAX_LEN];
    char subject_id[SUBJECT_ID_MAX_LEN];
    char relation[RELATION_MAX_LEN];
} DeleteRequest;

/*
 * Request payload (tagged union)
 *
 * Use base.type to determine which struct to access
 */
typedef union RequestPayload
{
    BaseRequest base;
    CheckRequest check;
    WriteRequest write;
    DeleteRequest delete_req;
} RequestPayload;

#endif /* POSTFGA_REQUEST_H */