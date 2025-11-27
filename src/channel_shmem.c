
#include <postgres.h>
#include <storage/shmem.h>

#include "channel.h"
#include "channel_shmem.h"

static Size
channel_shmem_size()
{
    return MAXALIGN(sizeof(FgaChannel));
}

static Size
pool_shmem_size(uint32 capacity)
{
    return MAXALIGN(offsetof(FgaChannelSlotPool, slots) + sizeof(FgaChannelSlot) * capacity);
}

static Size
queue_shmem_size(uint32 capacity)
{
    return MAXALIGN(offsetof(FgaChannelSlotQueue, values) + sizeof(uint16_t) * capacity);
}

Size postfga_channel_shmem_size(uint32 slot_count)
{
    elog(LOG, "postfga: init channel with slot_count=%u", slot_count);

    Size size = 0;

    // channel
    size = add_size(size, channel_shmem_size());

    // pool
    size = add_size(size, pool_shmem_size(slot_count));

    // queue
    size = add_size(size, queue_shmem_size(slot_count));
    return size;
}

void postfga_channel_shmem_init(FgaChannel *ch, uint32 slot_count)
{
    FgaChannelSlotPool *pool;
    FgaChannelSlotQueue *queue;

    // Channel struct
    char *ptr = (char *)ch + channel_shmem_size();

    // pool
    pool = (FgaChannelSlotPool *)ptr;
    ptr += pool_shmem_size(slot_count);

    // queue
    queue = (FgaChannelSlotQueue *)ptr;
    ptr += queue_shmem_size(slot_count);

    Assert(ptr == (char *)ch + size);

    ch->pool = pool;
    ch->queue = queue;

    pg_atomic_init_u64(&ch->request_id, 0);
    pool_init(ch->pool, slot_count);
    queue_init(ch->queue, slot_count);
}