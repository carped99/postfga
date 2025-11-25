extern "C"
{
#include <postgres.h>
#include <storage/lwlock.h>

#include "shmem.h"
}

#include <utility>
#include "processor.hpp"
#include "client/client_factory.hpp"
#include "util/logger.hpp"

namespace postfga::bgw
{
    static constexpr uint16_t MAX_BATCH = 50;

    namespace
    {
        // uint16_t drainSlots(PostfgaShmemState *state, FgaRequestSlot **out)
        // {
        //     uint16_t buf[MAX_BATCH];

        //     LWLockAcquire(state->lock, LW_EXCLUSIVE);
        //     auto count = postfga_dequeue_slots(state->request_queue, buf, MAX_BATCH);
        //     LWLockRelease(state->lock);

        //     FgaRequestSlot *slots = state->request_slots;
        //     for (uint16_t i = 0; i < count; ++i)
        //     {
        //         // pg_read_barrier(); // slot 읽기 전에 상태 변경이 완료되었는지 보장
        //         out[i] = &slots[buf[i]];
        //     }

        //     return count;
        // }
    } // namespace

    Processor::Processor(const postfga::Config &config)
    {
        this->client_ = postfga::client::make_client(config);
        // 추가 초기화가 필요하면 여기에 작성
    }

    void Processor::execute()
    {
        PostfgaShmemState *state = postfga_get_shmem_state();
        if (state == nullptr)
        {
            postfga::warning("PostFGA BGW: shared memory state is not initialized");
            return;
        }

        // FgaRequestSlot *slots[MAX_BATCH];
        // auto count = drainSlots(state, slots);
        // if (count == 0)
        //     return;

        // postfga::info("PostFGA BGW: processing " + std::to_string(count) + " requests");

        // for (uint16_t i = 0; i < count; ++i)
        // {
        //     auto slot = slots[i];
        //     uint32 expected = FGA_SLOT_PENDING;
        //     if (!pg_atomic_compare_exchange_u32(&slot->state,
        //                                         &expected,
        //                                         FGA_SLOT_PROCESSING))
        //     {
        //         // 상태가 이미 바뀌었으면 스킵
        //         continue;
        //     }

        //     FgaRequest &req = slot->request;
        //     FgaResponse &resp = slot->response;

        //     pg_write_barrier();
        //     pg_atomic_write_u32(&slot->state, FGA_SLOT_DONE);

        //     // client_.process(req, [&resp, slot](const FgaResponse &response)
        //     //                 {
        //     //     resp = response;
        //     //     pg_atomic_write_u32(&slot->state, FGA_SLOT_DONE); });
        // }
        // handle_batch(slots, count);
    }

    // void Processor::handle_batch(RequestPayload *requests, uint32_t count)
    // {
    //     for (uint32_t i = 0; i < count; ++i)
    //         handle_single_request(requests[i]);
    // }

    // void Processor::handle_single_request(RequestPayload &payload)
    // {
    //     switch (payload.base.type) {
    //         case REQ_TYPE_CHECK:
    //             handle_check(payload);
    //             break;
    //         case REQ_TYPE_WRITE:
    //             handle_write(payload);
    //             break;
    //         default:
    //             elog(WARNING, "PostFGA BGW: Unsupported request type %d", payload.base.type);
    //             // set_grpc_result(payload.base.request_id, false, 9999);
    //             break;
    //     }
    // }

    // void Processor::handle_check(RequestPayload &payload)
    // {
    // }

    // void Processor::handle_write(RequestPayload &payload)
    // {
    // }

} // namespace postfga::bgw