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
        ereport(LOG, errmsg("PostFGA BGW: Processor initialized"));
    }

    void Processor::execute()
    {
        FgaChannel* channel = channel_;
        FgaChannelSlot* slots[50];
        uint16_t count = postfga_channel_drain_slots(channel, 50, slots);
        if (count == 0)
            return;

        for (uint16_t i = 0; i < count; ++i)
        {
            FgaChannelSlot* slot = slots[i];
            uint32_t expected = FGA_CHANNEL_SLOT_PENDING;
            if (!pg_atomic_compare_exchange_u32(&slot->state, &expected, FGA_CHANNEL_SLOT_PROCESSING))
            {
                if (expected == FGA_CHANNEL_SLOT_CANCELED)
                {
                    /* 백엔드가 이미 이 요청을 포기함.
                     * → BGW가 여기서 슬롯 정리.
                     */
                    postfga_channel_release_slot(channel, slot);
                }
                else
                {
                    // 상태가 이미 바뀌었으면 스킵
                    ereport(WARNING, errmsg("PostFGA BGW: slot state changed unexpectedly"));
                }
                continue;
            }

            try
            {
                client_->process(slot->request, slot->response, &Processor::handleResponse, slot);
            }
            catch (const std::exception& e)
            {
                handleException(*slot, e.what());
            }
            catch (...)
            {
                handleException(*slot, "unknown error in processing request");
            }
        }
    }

    void Processor::complete_check(FgaChannelSlot& slot, const FgaResponse& resp)
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

    void Processor::handleResponse(const FgaResponse& resp, void* ctx) noexcept
    {
        auto& slot = *static_cast<FgaChannelSlot*>(ctx);

        try
        {
            complete_check(slot, resp); // 멤버 함수 호출
        }
        catch (const std::exception& e)
        {
            postfga::util::error("PostFGA BGW: complete_check failed: {}");
            // slot 에 에러 상태 기록 등
        }
        catch (...)
        {
            postfga::util::error("PostFGA BGW: complete_check threw unknown exception");
        }
    }

    void Processor::handleException(FgaChannelSlot& slot, const char* msg) noexcept
    {
        ereport(WARNING, errmsg("postfga: exception in processing request: %s", msg ? msg : "unknown"));

        FgaResponse& resp = slot.response;
        MemSet(&resp, 0, sizeof(resp));
        resp.status = FGA_RESPONSE_SERVER_ERROR;
        if (msg && *msg)
        {
            // constexpr std::size_t capacity = sizeof(resp.error_message);
            // std::size_t len = std::strnlen(msg, capacity - 1); // 최대 capacity-1까지만 검사
            // std::memcpy(resp.error_message, msg, len);

            std::strncpy(resp.error_message, msg, sizeof(resp.error_message) - 1);
            resp.error_message[sizeof(resp.error_message) - 1] = '\0';
        }
        else
        {
            resp.error_message[0] = '\0';
        }
        complete_check(slot, resp);
    }

    void Processor::wakeBackend(const FgaChannelSlot& slot)
    {
        auto pid = slot.backend_pid;
        if (pid <= 0)
        {
            postfga_channel_release_slot(channel_, const_cast<FgaChannelSlot*>(&slot));
            return;
        }


        auto proc = BackendPidGetProc(pid);
        if (proc == nullptr)
        {
            postfga_channel_release_slot(channel_, const_cast<FgaChannelSlot*>(&slot));
            return;
        }

        SetLatch(&proc->procLatch);
    }
} // namespace postfga::bgw
