/* slot.h */
#ifndef POSTFGA_CHECK_TUPLE_CHANNEL_H
#define POSTFGA_CHECK_TUPLE_CHANNEL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>
#include <port/atomics.h>
#include <storage/lwlock.h>
#include <lib/ilist.h>

#include "fga_type.h"
#include "check_slot.h"
#include "check_queue.h"

#ifdef __cplusplus
}
#endif

typedef struct FgaCheckChannel
{
    LWLock *lock;
    FgaCheckSlotPool *pool;
    FgaCheckSlotQueue *queue;
} FgaCheckChannel;

#ifdef __cplusplus
extern "C"
{
#endif
    uint16 postfga_channel_drain_slots(FgaCheckChannel *channel,
                                       uint16_t max_count,
                                       FgaCheckSlot **out_slots);

    bool postfga_check_execute(const FgaCheckTupleRequest *request);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHECK_TUPLE_CHANNEL_H */