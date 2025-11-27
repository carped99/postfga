/* slot.h */
#ifndef POSTFGA_CHECK_TUPLE_CHANNEL_H
#define POSTFGA_CHECK_TUPLE_CHANNEL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

#include <lib/ilist.h>
#include <port/atomics.h>
#include <storage/lwlock.h>

#include "channel_queue.h"
#include "channel_slot.h"
#include "request.h"

#ifdef __cplusplus
}
#endif

typedef struct FgaChannel
{
    LWLock* lock;
    pg_atomic_uint64 request_id; /* Request identifier */
    FgaChannelSlotPool* pool;
    FgaChannelSlotQueue* queue;
} FgaChannel;
#ifdef __cplusplus
extern "C"
{
#endif
    uint16 postfga_channel_drain_slots(FgaChannel* channel, uint16_t max_count, FgaChannelSlot** out_slots);

    FgaResponse* postfga_channel_execute(const FgaRequest* request);

    bool postfga_channel_check(const char* object_type,
                               const char* object_id,
                               const char* subject_type,
                               const char* subject_id,
                               const char* relation);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHECK_TUPLE_CHANNEL_H */
