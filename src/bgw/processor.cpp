extern "C"
{
#include <postgres.h>

#include <miscadmin.h>
#include <storage/lwlock.h>
#include <storage/proc.h>
#include <storage/procarray.h>

#include "shmem.h"
}

#include <cstring>
#include <utility>

#include "config/config.hpp"
#include "channel.h"
#include "payload.h"
#include "client/client.hpp"
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

        if (count == 1) {
            FgaChannelSlot* slot = slots[0];
            if (beginProcessing(*slot)) {
                client_->process(slot->payload, [this, slot]()
                {
                    enqueueCompleted(slot);
                });
            }
        } else if (count > 1) {
            postfga::client::ProcessItem items[MAX_BATCH];
            uint16_t batch_count = 0;

            for (uint16_t i = 0; i < count; ++i)
            {
                FgaChannelSlot* slot = slots[i];

                if (!beginProcessing(*slot))
                    continue;

                // client_->process(slot->request, slot->response, [this, slot]()
                // {
                //     completeProcessing(slot);
                // });

                items[batch_count].payload  = &slot->payload;
                items[batch_count].callback = [this, slot]()
                {
                    enqueueCompleted(slot);
                };                
                ++batch_count;
            }

            if (batch_count > 0)
            {
                std::span<postfga::client::ProcessItem> span(items, batch_count);
                client_->process_batch(span);
            }
        }

        drainCompleted();
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

    // 완료된 슬롯을 BGW 메인 루프에 알림 (외부 Thread에서 호출되므로 절대 postgresql 함수 호출 금지)
    void Processor::enqueueCompleted(FgaChannelSlot* slot) noexcept
    {
        {
            std::lock_guard<std::mutex> lock(completed_mu_);
            completed_queue_.push_back(slot);
        }
        SetLatch(MyLatch);
    }

    void Processor::drainCompleted() noexcept
    {
        std::vector<FgaChannelSlot*> local;
        {
            std::lock_guard<std::mutex> lock(completed_mu_);
            if (completed_queue_.empty())
            {
                return;
            }

            local.swap(completed_queue_);
            completed_queue_.reserve(10);
        }

        for (auto* slot : local)
        {
            handleResponse(*slot);
        }
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

        FgaResponse& resp = slot.payload.response;
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
        if (!postfga_channel_wake_backend(&slot))
        {
            postfga_channel_release_slot(channel_, &slot);
        }
    }
} // namespace postfga::bgw
