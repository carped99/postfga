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
    FgaCheckSlot *postfga_check_write(FgaCheckChannel *channel,
                                      const FgaCheckTupleRequest *request);
    void postfga_check_read(FgaCheckChannel *channel,
                            FgaCheckSlot *slot);

    Size postfga_check_channel_shmem_size(uint32 slot_count);
    void postfga_check_channel_shmem_init(uint32 slot_count);
#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHECK_TUPLE_CHANNEL_H */