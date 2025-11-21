/*-------------------------------------------------------------------------
 *
 * queue.h
 *    Request queue operations for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTFGA_QUEUE_H
#define POSTFGA_QUEUE_H


#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <postgres.h>
#include <datatype/timestamp.h>
#include <storage/latch.h>

#include "request.h"

/* -------------------------------------------------------------------------
 * Request queue operations (stub for future use)
 * -------------------------------------------------------------------------
 */

/* Enqueue a gRPC request */
uint32 enqueue_grpc_request(const char *object_type, const char *object_id,
                           const char *subject_type, const char *subject_id,
                           const char *relation);

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

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_QUEUE_H */
