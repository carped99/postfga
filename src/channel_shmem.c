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

static Size pool_shmem_size(uint32 capacity)
{
    Size size = offsetof(FgaChannelSlotPool, slots);
    size = add_size(size, mul_size(sizeof(FgaChannelSlot), capacity));
    return size;
}

static Size queue_shmem_size(uint32 capacity)
{
    Size size = offsetof(FgaChannelSlotQueue, values);
    size = add_size(size, mul_size(sizeof(FgaChannelSlotIndex), capacity));
    return size;
}

static int compute_slot_size(void)
{
    const char* max_conn_str;
    PostfgaConfig* cfg = postfga_get_config();
    if (cfg->max_slots > 0)
        return cfg->max_slots;

    max_conn_str = GetConfigOption("max_connections", false, false);
    int max_conn = max_conn_str ? atoi(max_conn_str) : 100; /* fallback */
    int slots = max_conn * 2;

    if (slots < 1024)
        slots = 1024;
    if (slots > 16384)
        slots = 16384; /* 필요시 더 키워도 됨 */

    return slots;
}

Size postfga_channel_shmem_size(void)
{
    uint32 slot_count = compute_slot_size();
    uint32 queue_capacity = pow2_ceil(slot_count);
    Size size = 0;

    // channel struct itself
    size = add_size(size, MAXALIGN(sizeof(FgaChannel)));

    // pool
    size = add_size(size, MAXALIGN(pool_shmem_size(slot_count)));

    // queue
    size = add_size(size, MAXALIGN(queue_shmem_size(queue_capacity)));

    return size;
}

void postfga_channel_shmem_init(FgaChannel* ch, LWLock* pool_lock, LWLock* queue_lock)
{
    FgaChannelSlotPool* pool;
    FgaChannelSlotQueue* queue;

    uint32 slot_count = compute_slot_size();
    uint32 queue_capacity = pow2_ceil(slot_count);

    ch->pool_lock = pool_lock;
    ch->queue_lock = queue_lock;

    // Channel struct
    char* ptr = (char*)ch + MAXALIGN(sizeof(FgaChannel));

    // pool
    pool = (FgaChannelSlotPool*)ptr;
    ptr += MAXALIGN(pool_shmem_size(slot_count));

    // queue
    queue = (FgaChannelSlotQueue*)ptr;
    ptr += MAXALIGN(queue_shmem_size(queue_capacity));

    ch->pool = pool;
    ch->queue = queue;

    pg_atomic_init_u64(&ch->request_id, 0);
    pool_init(ch->pool, slot_count);
    queue_init(ch->queue, queue_capacity);

    if (unlikely(ptr != (char*)ch + MAXALIGN(postfga_channel_shmem_size())))
    {
        ereport(FATAL, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("postfga: channel shmem size miscalculation")));
    }

    {
        const Size size = postfga_channel_shmem_size();
        ereport(LOG,
                errcode(ERRCODE_SUCCESSFUL_COMPLETION),
                errmsg("postfga: channel initialized"),
                errdetail("slot_count=%u, queue_capacity=%u, total_size=%zu", slot_count, queue_capacity, size));

        ereport(LOG, errmsg("sizeof(FgaTuple) = %zu", sizeof(FgaTuple)));
        ereport(LOG, errmsg("sizeof(FgaRequest) = %zu", sizeof(FgaRequest)));
        ereport(LOG, errmsg("sizeof(FgaResponse) = %zu", sizeof(FgaResponse)));
        ereport(LOG, errmsg("sizeof(FgaPayload) = %zu", sizeof(FgaPayload)));
    }
}
