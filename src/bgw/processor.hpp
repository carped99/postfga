// bgw_processor.hpp
#pragma once

extern "C" {
#include "postgres.h"
}

#include "queue.h"
// #include "client_adapter.hpp"  // gRPC client 래핑
#include "guc.h"

namespace postfga::bgw {

class Processor {
public:
    Processor();
    void execute();

private:
    static constexpr uint32_t MAX_BATCH_SIZE = 32;

    bool load_config();
    void handle_batch(RequestPayload *requests, uint32_t count);
    void handle_single_request(RequestPayload &payload);

    void handle_check(RequestPayload &payload);
    void handle_write(RequestPayload &payload);

    // 설정 캐시
    const char *store_id_ = nullptr;
    const char *auth_model_id_ = nullptr;

    // ClientAdapter client_;    // gRPC 클라이언트 래퍼
};

} // namespace postfga::bgw