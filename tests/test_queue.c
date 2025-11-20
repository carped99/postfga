/*
 * test_queue.c
 *
 * Test program for request queue - PostgreSQL extension context
 *
 * This simulates what would happen inside PostgreSQL when using the queue
 */

#include <postgres.h>
#include <fmgr.h>
#include <funcapi.h>
#include <miscadmin.h>
#include <utils/timestamp.h>
#include <utils/builtins.h>

#include "queue.h"
#include "state.h"

/*
 * SQL function: postfga_test_enqueue(count int)
 *
 * Enqueues 'count' test requests to the queue
 */
PG_FUNCTION_INFO_V1(postfga_test_enqueue);
Datum
postfga_test_enqueue(PG_FUNCTION_ARGS)
{
    int32 count = PG_GETARG_INT32(0);
    int32 i;
    int32 success_count = 0;

    elog(LOG, "PostFGA Test: Enqueuing %d test requests", count);

    for (i = 0; i < count; i++)
    {
        char object_id[64];
        char subject_id[64];
        uint32 request_id;

        /* Generate test data */
        snprintf(object_id, sizeof(object_id), "document-%d", i);
        snprintf(subject_id, sizeof(subject_id), "user-%d", i % 10);

        /* Enqueue request */
        request_id = enqueue_grpc_request(
            "document",           /* object_type */
            object_id,            /* object_id */
            "user",               /* subject_type */
            subject_id,           /* subject_id */
            "reader"              /* relation */
        );

        if (request_id > 0)
        {
            success_count++;
            elog(DEBUG1, "PostFGA Test: Enqueued request #%u: %s:%s#reader@user:%s",
                 request_id, "document", object_id, subject_id);
        }
        else
        {
            elog(WARNING, "PostFGA Test: Failed to enqueue request %d", i);
        }
    }

    elog(LOG, "PostFGA Test: Successfully enqueued %d/%d requests",
         success_count, count);

    PG_RETURN_INT32(success_count);
}

/*
 * SQL function: postfga_test_enqueue_continuous()
 *
 * Background process that enqueues one request per second
 * Returns after 10 requests
 */
PG_FUNCTION_INFO_V1(postfga_test_enqueue_continuous);
Datum
postfga_test_enqueue_continuous(PG_FUNCTION_ARGS)
{
    int32 count = 0;
    int32 max_requests = 10;

    elog(LOG, "PostFGA Test: Starting continuous enqueue (max %d requests)", max_requests);

    while (count < max_requests)
    {
        char object_id[64];
        char subject_id[64];
        uint32 request_id;

        CHECK_FOR_INTERRUPTS();

        /* Generate test data */
        snprintf(object_id, sizeof(object_id), "document-%d", count);
        snprintf(subject_id, sizeof(subject_id), "user-alice");

        /* Enqueue request */
        request_id = enqueue_grpc_request(
            "document",
            object_id,
            "user",
            subject_id,
            "reader"
        );

        if (request_id > 0)
        {
            elog(LOG, "PostFGA Test: [%d/%d] Enqueued request #%u",
                 count + 1, max_requests, request_id);
        }
        else
        {
            elog(WARNING, "PostFGA Test: Failed to enqueue request");
        }

        count++;

        /* Sleep for 1 second */
        pg_usleep(1000000L); /* 1 second = 1,000,000 microseconds */
    }

    elog(LOG, "PostFGA Test: Completed %d requests", count);

    PG_RETURN_INT32(count);
}

/*
 * SQL function: postfga_test_queue_stats()
 *
 * Returns current queue statistics
 */
PG_FUNCTION_INFO_V1(postfga_test_queue_stats);
Datum
postfga_test_queue_stats(PG_FUNCTION_ARGS)
{
    uint32 size = 0;
    uint32 capacity = 0;
    bool success;
    char result[256];

    success = get_queue_stats(&size, &capacity);

    if (success)
    {
        snprintf(result, sizeof(result),
                 "Queue: %u/%u (%.1f%% full)",
                 size, capacity,
                 capacity > 0 ? (100.0 * size / capacity) : 0.0);
    }
    else
    {
        snprintf(result, sizeof(result), "Failed to get queue stats");
    }

    PG_RETURN_TEXT_P(cstring_to_text(result));
}

/*
 * SQL function: postfga_test_clear_queue()
 *
 * Clears completed requests from queue
 */
PG_FUNCTION_INFO_V1(postfga_test_clear_queue);
Datum
postfga_test_clear_queue(PG_FUNCTION_ARGS)
{
    uint32 cleared;

    cleared = clear_completed_requests();

    elog(LOG, "PostFGA Test: Cleared %u completed requests", cleared);

    PG_RETURN_INT32(cleared);
}

/*
 * SQL function: postfga_write_tuple(object_type, object_id, subject_type, subject_id, relation)
 *
 * Write a tuple to OpenFGA via BGW queue
 */
PG_FUNCTION_INFO_V1(postfga_write_tuple);
Datum
postfga_write_tuple(PG_FUNCTION_ARGS)
{
    char *object_type;
    char *object_id;
    char *subject_type;
    char *subject_id;
    char *relation;
    uint32 request_id;
    bool allowed;
    uint32 error_code;

    /* Get arguments */
    object_type = text_to_cstring(PG_GETARG_TEXT_PP(0));
    object_id = text_to_cstring(PG_GETARG_TEXT_PP(1));
    subject_type = text_to_cstring(PG_GETARG_TEXT_PP(2));
    subject_id = text_to_cstring(PG_GETARG_TEXT_PP(3));
    relation = text_to_cstring(PG_GETARG_TEXT_PP(4));

    elog(LOG, "PostFGA: Writing tuple %s:%s#%s@%s:%s",
         object_type, object_id, relation, subject_type, subject_id);

    /* Enqueue write request */
    request_id = enqueue_write_request(object_type, object_id, subject_type, subject_id, relation);

    if (request_id == 0)
    {
        elog(ERROR, "PostFGA: Failed to enqueue write request");
    }

    elog(LOG, "PostFGA: Enqueued write request #%u, waiting for result...", request_id);

    /* Wait for result from BGW */
    if (!wait_for_grpc_result(request_id, &allowed, &error_code))
    {
        elog(ERROR, "PostFGA: Write request timed out");
    }

    if (!allowed)
    {
        elog(ERROR, "PostFGA: Write request returned false (error_code: %u)", error_code);
    }

    elog(LOG, "PostFGA: Write tuple completed successfully");

    PG_RETURN_BOOL(true);
}

/*
 * SQL function: postfga_check(object_type, object_id, subject_type, subject_id, relation)
 *
 * Check permission via BGW queue
 */
PG_FUNCTION_INFO_V1(postfga_check);
Datum
postfga_check(PG_FUNCTION_ARGS)
{
    char *object_type;
    char *object_id;
    char *subject_type;
    char *subject_id;
    char *relation;
    uint32 request_id;
    bool allowed;
    uint32 error_code;

    /* Get arguments */
    object_type = text_to_cstring(PG_GETARG_TEXT_PP(0));
    object_id = text_to_cstring(PG_GETARG_TEXT_PP(1));
    subject_type = text_to_cstring(PG_GETARG_TEXT_PP(2));
    subject_id = text_to_cstring(PG_GETARG_TEXT_PP(3));
    relation = text_to_cstring(PG_GETARG_TEXT_PP(4));

    elog(DEBUG1, "PostFGA: Checking permission %s:%s#%s@%s:%s",
         object_type, object_id, relation, subject_type, subject_id);

    /* Enqueue check request */
    request_id = enqueue_grpc_request(object_type, object_id, subject_type, subject_id, relation);

    if (request_id == 0)
    {
        elog(ERROR, "PostFGA: Failed to enqueue check request");
    }

    elog(DEBUG2, "PostFGA: Enqueued check request #%u, waiting for result...", request_id);

    /* Wait for result from BGW */
    if (!wait_for_grpc_result(request_id, &allowed, &error_code))
    {
        elog(ERROR, "PostFGA: Check request timed out");
    }

    if (error_code != 0)
    {
        elog(ERROR, "PostFGA: Check request failed with error_code: %u", error_code);
    }

    PG_RETURN_BOOL(allowed);
}
