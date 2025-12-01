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
        bool beginProcessing(FgaChannelSlot& slot) noexcept;
        void completeProcessing(FgaChannelSlot& slot) noexcept;
        void handleResponse(FgaChannelSlot& slot);
        void handleException(FgaChannelSlot& slot, const char* msg) noexcept;
        void wakeBackend(FgaChannelSlot& slot);

      private:
        static constexpr uint32_t MAX_BATCH_SIZE = 32;
        FgaChannel* channel_ = nullptr;
        std::shared_ptr<postfga::client::Client> client_;
        postfga::util::Counter inflight_;
    };

} // namespace postfga::bgw
