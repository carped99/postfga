/* slot.h */
#ifndef POSTFGA_CHANNEL_H
#define POSTFGA_CHANNEL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

#include <lib/ilist.h>
#include <port/atomics.h>
#include <storage/lwlock.h>

#include "payload.h"

#ifdef __cplusplus
}
#endif

typedef uint16_t FgaChannelSlotIndex;

typedef enum FgaChannelSlotState
{
    FGA_CHANNEL_SLOT_EMPTY = 0,
    FGA_CHANNEL_SLOT_CANCELED,
    FGA_CHANNEL_SLOT_PENDING,
    FGA_CHANNEL_SLOT_PROCESSING,
    FGA_CHANNEL_SLOT_DONE
} FgaChannelSlotState;

typedef struct FgaChannelSlot
{
    slist_node node;        /* 내부 연결 리스트 용도 */
    pg_atomic_uint32 state; /* FgaChannelSlotState */
    pid_t backend_pid;      /* 요청한 백엔드 PID */
    FgaPayload payload;     /* 요청 내용 */
} FgaChannelSlot;

typedef struct FgaChannelSlotPool
{
    slist_head head;
    FgaChannelSlot slots[FLEXIBLE_ARRAY_MEMBER];
} FgaChannelSlotPool;

typedef struct FgaChannelSlotQueue
{
    uint16_t mask;                                     /* capacity - 1 (2^n - 1) */
    uint16_t head;                                     /* enqueue index */
    uint16_t tail;                                     /* dequeue index */
    FgaChannelSlotIndex values[FLEXIBLE_ARRAY_MEMBER]; /* [capacity = mask + 1] */
} FgaChannelSlotQueue;


typedef struct FgaChannel
{
    LWLock* pool_lock;
    LWLock* queue_lock;
    pg_atomic_uint64 request_id; /* Request identifier */
    FgaChannelSlotPool* pool;
    FgaChannelSlotQueue* queue;
} FgaChannel;

#ifdef __cplusplus
extern "C"
{
#endif
    uint16 postfga_channel_drain_slots(FgaChannel* const channel, uint16_t max_count, FgaChannelSlot** out_slots);

    void postfga_channel_release_slot(FgaChannel* const channel, FgaChannelSlot* const slot);

    void postfga_channel_execute(const FgaRequest* const request, FgaResponse* const response);

    bool postfga_channel_wake_backend(const FgaChannelSlot* const slot);
#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHANNEL_H */
