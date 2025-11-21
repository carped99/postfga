// processor.hpp
#pragma once

#include "client/client.hpp"

namespace postfga::bgw {

class Processor {
public:
    explicit Processor(PostfgaShmemState *state, const postfga::client::Config &config);
    void execute();

private:
    static constexpr uint32_t MAX_BATCH_SIZE = 32;

    void handle_batch(RequestPayload *requests, uint32_t count);
    void handle_single_request(RequestPayload &payload);

    void handle_check(RequestPayload &payload);
    void handle_write(RequestPayload &payload);

    PostfgaShmemState *state_ = nullptr;
    postfga::client::Client client_;
};

} // namespace postfga::bgw