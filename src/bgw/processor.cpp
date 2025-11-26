extern "C"
{
#include <postgres.h>
#include <storage/lwlock.h>
#include <storage/proc.h>
#include <storage/procarray.h>

#include "shmem.h"
}

#include <utility>
#include "processor.hpp"
#include "client/client_factory.hpp"
#include "util/logger.hpp"
#include "check_channel.h"
#include "task_slot.hpp"

namespace postfga::bgw
{
    static constexpr uint16_t MAX_BATCH = 50;

    // static GrpcAsyncSlotPool task_pool(100);

    namespace
    {
        void wakeBackend(FgaCheckSlot *slot)
        {
            auto pid = slot->backend_pid;
            if (pid <= 0)
            {
                ereport(WARNING,
                        errmsg("invalid backend pid=%d", pid));
                return;
            }

            auto proc = BackendPidGetProc(pid);
            if (proc == nullptr)
            {
                ereport(WARNING,
                        errmsg("backend %d no longer exists", pid));
                return;
            }

            // if (&proc->procLatch != slot->latch)
            // {
            //     ereport(WARNING,
            //             errmsg("backend pid=%d has different latch", pid));
            //     return;
            // }
            SetLatch(&proc->procLatch);
        }

        void complete_check(FgaCheckSlot *slot, const FgaCheckTupleResponse &resp)
        {
            // slot->response = resp;
            pg_write_barrier();
            pg_atomic_write_u32(&slot->state, FGA_CHECK_SLOT_DONE);
            wakeBackend(slot);
        }
    } // namespace

    Processor::Processor(const postfga::Config &config)
        : client_(postfga::client::make_client(config)), inflight_(1000)
    {
    }

    void Processor::execute()
    {
        PostfgaShmemState *state = postfga_get_shmem_state();
        if (state == nullptr)
        {
            postfga::warning("PostFGA BGW: shared memory state is not initialized");
            return;
        }

        uint16_t count = 0;
        FgaCheckSlot *slots[50];
        count = postfga_channel_drain_slots(state->check_channel,
                                            50,
                                            slots);
        if (count == 0)
            return;

        postfga::info("PostFGA BGW: processing " + std::to_string(count) + " requests");

        for (uint16_t i = 0; i < count; ++i)
        {
            auto slot = slots[i];
            uint32_t expected = FGA_CHECK_SLOT_PENDING;
            if (!pg_atomic_compare_exchange_u32(&slot->state,
                                                &expected,
                                                FGA_CHECK_SLOT_PROCESSING))
            {
                // 상태가 이미 바뀌었으면 스킵
                ereport(WARNING,
                        errmsg("PostFGA BGW: slot state changed unexpectedly"));
                continue;
            }

            // task_pool.acquire();
        }
    }

    FgaCheckTupleResponse Processor::check(const FgaCheckTupleRequest &request)
    {
        FgaCheckTupleResponse resp{};
        return resp;
        // return client_->process(request);
    }

    void Processor::check_async(const FgaCheckTupleRequest &request, FgaCheckTupleResponseHandler handler)
    {
        auto self = shared_from_this();
        // task_pool.release(nullptr);
        // asio::post(pool_,
        //            [self, req = request, h = std::move(handler)]() mutable
        //            {
        //                FgaCheckTupleResponse resp;
        //                try
        //                {
        //                    resp = self->check(req);
        //                }
        //                catch (...)
        //                {
        //                    resp = make_internal_error_response(); // 짧게 감춤
        //                }
        //                h(resp);
        //            });
    }
} // namespace postfga::bgw