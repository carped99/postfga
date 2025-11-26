
#include <postgres.h>
#include <storage/shmem.h>

#include "check_channel.h"
#include "check_shmem.h"

static Size
channel_shmem_size()
{
    return MAXALIGN(sizeof(FgaCheckChannel));
}

static Size
pool_shmem_size(uint32 capacity)
{
    return MAXALIGN(offsetof(FgaCheckSlotPool, slots) + sizeof(FgaCheckSlot) * capacity);
}

static Size
queue_shmem_size(uint32 capacity)
{
    return MAXALIGN(offsetof(FgaCheckSlotQueue, values) + sizeof(uint16) * capacity);
}

Size postfga_check_channel_shmem_size(uint32 slot_count)
{
    elog(LOG, "PostFGA: init check channel with slot_count=%u", slot_count);

    Size size = channel_shmem_size();

    // pool
    size = add_size(size, pool_shmem_size(slot_count));

    // queue
    size = add_size(size, queue_shmem_size(slot_count));
    return size;
}

void postfga_check_channel_shmem_init(FgaCheckChannel *ch, uint32 slot_count)
{
    FgaCheckSlotPool *pool;
    FgaCheckSlotQueue *queue;

    // Channel struct
    char *ptr = (char *)ch + channel_shmem_size();

    // pool
    pool = (FgaCheckSlotPool *)ptr;
    ptr += pool_shmem_size(slot_count);

    // queue
    queue = (FgaCheckSlotQueue *)ptr;
    ptr += queue_shmem_size(slot_count);

    Assert(ptr == (char *)ch + size);

    ch->pool = pool;
    ch->queue = queue;

    pool_init(ch->pool, slot_count);
    queue_init(ch->queue, slot_count);
}