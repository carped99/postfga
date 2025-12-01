// processor.hpp
#pragma once

#include <memory>

#include "client/client.hpp"
#include "config/config.hpp"
#include "util/counter.hpp"

struct FgaChannel;
struct FgaChannelSlot;

namespace postfga::bgw
{
    class Processor
    {
      public:
        explicit Processor(FgaChannel* channel, const postfga::Config& config);
        void execute();

      private:
        void wakeBackend(const FgaChannelSlot& slot);
        void complete_check(FgaChannelSlot& slot, const FgaResponse& resp);
        void handleResponse(const FgaResponse& resp, void* ctx) noexcept;
        void handleException(FgaChannelSlot& slot, const char* msg) noexcept;

      private:
        static constexpr uint32_t MAX_BATCH_SIZE = 32;
        FgaChannel* channel_ = nullptr;
        std::shared_ptr<postfga::client::Client> client_;
        postfga::util::Counter inflight_;
    };

} // namespace postfga::bgw
