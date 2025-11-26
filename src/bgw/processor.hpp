// processor.hpp
#pragma once

#include <memory>
#include "config/config.hpp"
#include "client/client.hpp"
#include "util/counter.hpp"

struct FgaCheckTupleRequest;

namespace postfga::bgw
{

    class Processor : public std::enable_shared_from_this<Processor>
    {
    public:
        explicit Processor(const postfga::Config &config);
        void execute();

    private:
        using FgaCheckTupleResponseHandler = std::function<void(const FgaCheckTupleResponse &)>;

        static constexpr uint32_t MAX_BATCH_SIZE = 32;

        FgaCheckTupleResponse check(const FgaCheckTupleRequest &request);
        void check_async(const FgaCheckTupleRequest &request, FgaCheckTupleResponseHandler handler);

        std::unique_ptr<postfga::client::Client> client_;
        postfga::util::Counter inflight_;
    };

} // namespace postfga::bgw