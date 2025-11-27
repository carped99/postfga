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
#include "channel_slot.h"
#include "channel_queue.h"

#ifdef __cplusplus
}
#endif

typedef struct FgaChannel
{
    LWLock *lock;
    FgaChannelSlotPool *pool;
    FgaChannelSlotQueue *queue;
} FgaChannel;
#ifdef __cplusplus
extern "C"
{
#endif
    uint16 postfga_channel_drain_slots(FgaChannel *channel,
                                       uint16_t max_count,
                                       FgaChannelSlot **out_slots);

    bool postfga_channel_execute(const FgaRequest *request);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHECK_TUPLE_CHANNEL_H */