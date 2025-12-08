// processor.hpp
#pragma once

#include <memory>

#include "client/client.hpp"
#include "config/config.hpp"
#include "util/counter.hpp"

struct FgaChannel;
struct FgaChannelSlot;

namespace fga::bgw
{
    class Processor
    {
      public:
        explicit Processor(FgaChannel* channel, const fga::Config& config);
        void execute();

      private:
        bool beginProcessing(FgaChannelSlot& slot) noexcept;
        void handleResponse(FgaChannelSlot& slot);
        void handleException(FgaChannelSlot& slot, const char* msg) noexcept;
        void wakeBackend(FgaChannelSlot& slot);

        void enqueueCompleted(FgaChannelSlot* slot) noexcept;
        void drainCompleted() noexcept;

      private:
        FgaChannel* channel_ = nullptr;
        std::shared_ptr<fga::client::Client> client_;
        fga::util::Counter inflight_;

        std::mutex completed_mu_;
        std::vector<FgaChannelSlot*> completed_queue_;
    };

} // namespace fga::bgw
