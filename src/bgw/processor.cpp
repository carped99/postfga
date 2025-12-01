extern "C"
{
#include <postgres.h>

#include <storage/lwlock.h>
#include <storage/proc.h>
#include <storage/procarray.h>

#include "shmem.h"
}

#include <cstring>
#include <utility>

#include "channel.h"
#include "client/client_factory.hpp"
#include "processor.hpp"
#include "util/logger.hpp"

namespace postfga::bgw
{
    static constexpr uint16_t MAX_BATCH = 50;

    Processor::Processor(FgaChannel* channel, const postfga::Config& config)
        : channel_(channel),
          client_(postfga::client::make_client(config)),
          inflight_(1000)
    {
    }

    void Processor::execute()
    {
        static constexpr uint16_t MAX_BATCH = 50;

        FgaChannel* channel = channel_;
        FgaChannelSlot* slots[MAX_BATCH];
        uint16_t count = postfga_channel_drain_slots(channel, MAX_BATCH, slots);

        postfga::client::ProcessItem items[MAX_BATCH];
        // FgaChannelSlot* batch_slots[MAX_BATCH];
        uint16_t batch_count = 0;

        for (uint16_t i = 0; i < count; ++i)
        {
            FgaChannelSlot* slot = slots[i];

            if (!beginProcessing(*slot))
                continue;

            items[batch_count].request  = &slot->request;
            items[batch_count].response = &slot->response;
            items[batch_count].callback = [this, slot]()
            {
                completeProcessing(slot);
            };                

            // batch_slots[batch_count] = slot;
            ++batch_count;
        }

        if (batch_count > 0)
        {
            ereport(LOG, errmsg("postfga: 3. processing batch of %u requests", batch_count));
            std::span<postfga::client::ProcessItem> span(items, batch_count);
            ereport(LOG, errmsg("postfga: 4. processing batch of %u requests", batch_count));
            client_->process_batch(span);
        }

        std::vector<FgaChannelSlot*> local;
        {
            std::lock_guard<std::mutex> lock(completed_mu_);
            if (completed_.empty())
                return;
            local.swap(completed_);
        }

        for (auto* slot : local)
        {
            handleResponse(*slot);
        }
    }

    bool Processor::beginProcessing(FgaChannelSlot& slot) noexcept
    {
        uint32_t expected = FGA_CHANNEL_SLOT_PENDING;

        if (pg_atomic_compare_exchange_u32(&slot.state, &expected, FGA_CHANNEL_SLOT_PROCESSING))
            return true;

        if (expected == FGA_CHANNEL_SLOT_CANCELED)
        {
            /*
             * 백엔드가 이미 이 요청을 포기함.
             * → BGW가 여기서 슬롯 정리.
             */
            postfga_channel_release_slot(channel_, &slot);
        }
        else
        {
            ereport(WARNING, errmsg("postfga: slot state changed unexpectedly (state=%u)", expected));
        }
        return false;
    }

    void Processor::completeProcessing(FgaChannelSlot* slot) noexcept
    {
        ereport(LOG, errmsg("postfga: marking slot as completed"));
        
        std::lock_guard<std::mutex> lock(completed_mu_);
        completed_.push_back(slot);
        SetLatch(MyLatch);
    }

    void Processor::handleResponse(FgaChannelSlot& slot)
    {
        auto state = (FgaChannelSlotState)pg_atomic_read_u32(&slot.state);

        if (state == FGA_CHANNEL_SLOT_CANCELED)
        {
            /* 백엔드가 이미 포기한 요청:
             * - Latch 깨울 필요 없음
             * - BGW가 대신 슬롯을 반환
             */
            postfga_channel_release_slot(channel_, &slot);
            return;
        }

        // 정상 완료된 요청 작업
        // slot->response = resp;
        pg_write_barrier();
        pg_atomic_write_u32(&slot.state, FGA_CHANNEL_SLOT_DONE);
        wakeBackend(slot);
    }

    void Processor::handleException(FgaChannelSlot& slot, const char* msg) noexcept
    {
        ereport(WARNING, errmsg("postfga: exception in processing request: %s", msg ? msg : "unknown"));

        FgaResponse& resp = slot.response;
        MemSet(&resp, 0, sizeof(resp));
        resp.status = FGA_RESPONSE_SERVER_ERROR;
        if (msg && *msg)
        {
            std::strncpy(resp.error_message, msg, sizeof(resp.error_message) - 1);
            resp.error_message[sizeof(resp.error_message) - 1] = '\0';
        }
        else
        {
            resp.error_message[0] = '\0';
        }
        handleResponse(slot);
    }

    void Processor::wakeBackend(FgaChannelSlot& slot)
    {
        auto pid = slot.backend_pid;
        if (pid <= 0)
        {
            postfga_channel_release_slot(channel_, &slot);
            return;
        }


        auto proc = BackendPidGetProc(pid);
        if (proc == nullptr)
        {
            postfga_channel_release_slot(channel_, &slot);
            return;
        }

        SetLatch(&proc->procLatch);
    }
} // namespace postfga::bgw
