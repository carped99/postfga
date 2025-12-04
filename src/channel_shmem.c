#include <postgres.h>

#include <storage/shmem.h>
#include <utils/guc.h>

#include "channel.h"
#include "channel_shmem.h"
#include "channel_slot.h"
#include "config.h"

static inline uint32 pow2_ceil(uint32 x)
{
    if (x == 0)
        return 1;

    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

static Size channel_shmem_size()
{
    return MAXALIGN(sizeof(FgaChannel));
}

static Size pool_shmem_size(uint32 capacity)
{
    return MAXALIGN(offsetof(FgaChannelSlotPool, slots) + sizeof(FgaChannelSlot) * capacity);
}

static Size queue_shmem_size(uint32 capacity)
{
    return MAXALIGN(offsetof(FgaChannelSlotQueue, values) + sizeof(uint16_t) * capacity);
}

static int compute_slot_size(void)
{
    PostfgaConfig* cfg = postfga_get_config();
    if (cfg->max_slots > 0)
        return cfg->max_slots;

    const char *val = GetConfigOption("max_connections", false, false);
    int max_conn = val ? atoi(val) : 100;  /* fallback */
    int slots = max_conn * 2;

    if (slots < 1024)
        slots = 1024;
    if (slots > 16384)
        slots = 16384;                     /* 필요시 더 키워도 됨 */

    return slots;
}

Size postfga_channel_shmem_size(void)
{
    uint32 slot_count = compute_slot_size();
    uint32 queue_capacity = pow2_ceil(slot_count);
    Size size = 0;

    // channel
    size = add_size(size, channel_shmem_size());

    // pool
    size = add_size(size, pool_shmem_size(slot_count));

    // queue

    size = add_size(size, queue_shmem_size(queue_capacity));


    ereport(LOG, errmsg("sizeof(FgaTuple) = %zu", sizeof(FgaTuple)));                             
    ereport(LOG, errmsg("sizeof(FgaRequest) = %zu", sizeof(FgaRequest)));
    ereport(LOG, errmsg("sizeof(FgaResponse) = %zu", sizeof(FgaResponse)));
    ereport(LOG, errmsg("sizeof(FgaPayload) = %zu", sizeof(FgaPayload)));
    ereport(LOG, errmsg("postfga: channel size: slot_count=%u, queue_capacity=%u, total_size=%zu",
                         slot_count,
                         queue_capacity,
                         size));
    return size;
}

void postfga_channel_shmem_init(FgaChannel* ch, LWLock* pool_lock, LWLock* queue_lock)
{
    FgaChannelSlotPool* pool;
    FgaChannelSlotQueue* queue;

    ch->pool_lock = pool_lock;
    ch->queue_lock = queue_lock;

    uint32 slot_count = compute_slot_size();
    uint32 queue_capacity = pow2_ceil(slot_count);

    // Channel struct
    char* ptr = (char*)ch + channel_shmem_size();

    // pool
    pool = (FgaChannelSlotPool*)ptr;
    ptr += pool_shmem_size(slot_count);

    // queue
    queue = (FgaChannelSlotQueue*)ptr;
    ptr += queue_shmem_size(queue_capacity);

    Assert(ptr == (char*)ch + size);

    ch->pool = pool;
    ch->queue = queue;

    pg_atomic_init_u64(&ch->request_id, 0);
    pool_init(ch->pool, slot_count);
    queue_init(ch->queue, queue_capacity);
}
