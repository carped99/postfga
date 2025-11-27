// processor.hpp
#pragma once

#include <memory>
#include "config/config.hpp"
#include "client/client.hpp"
#include "util/counter.hpp"

struct PostfgaShmemState;

namespace postfga::bgw
{
    class Processor : public std::enable_shared_from_this<Processor>
    {
    public:
        explicit Processor(PostfgaShmemState *state, const postfga::Config &config);
        void execute();

    private:
        static constexpr uint32_t MAX_BATCH_SIZE = 32;

        std::unique_ptr<postfga::client::Client> client_;
        postfga::util::Counter inflight_;
        PostfgaShmemState *state_ = nullptr;
    };

} // namespace postfga::bgw