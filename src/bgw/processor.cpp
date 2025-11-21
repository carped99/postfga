extern "C" {
#include "state.h"
#include "queue.h"
}

#include <utility>
#include "processor.hpp"

namespace postfga::bgw {

Processor::Processor(PostfgaShmemState *state, const postfga::client::Config &config)
      : state_(state)
      , client_(config)
{
    Assert(state != nullptr);
    // 추가 초기화가 필요하면 여기에 작성
}

void Processor::execute()
{
    // RequestPayload requests[MAX_BATCH_SIZE];
    // uint32_t count = MAX_BATCH_SIZE;

    // if (!client_.ensure_initialized()) {
    //     // 로그는 ClientAdapter 내부에서
    //     return;
    // }

    // if (!dequeue_requests(requests, &count) || count == 0)
    //     return;

    // if (!load_config()) {
    //     for (uint32_t i = 0; i < count; ++i)
    //         set_grpc_result(requests[i].base.request_id, false, 8888);
    //     return;
    // }

    // elog(DEBUG1, "PostFGA BGW: Processing %u requests", count);
    // handle_batch(requests, count);

    // client_.poll(0);
}

void Processor::handle_batch(RequestPayload *requests, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
        handle_single_request(requests[i]);
}

void Processor::handle_single_request(RequestPayload &payload)
{
    switch (payload.base.type) {
        case REQ_TYPE_CHECK:
            handle_check(payload);
            break;
        case REQ_TYPE_WRITE:
            handle_write(payload);
            break;
        default:
            elog(WARNING, "PostFGA BGW: Unsupported request type %d", payload.base.type);
            // set_grpc_result(payload.base.request_id, false, 9999);
            break;
    }
}

void Processor::handle_check(RequestPayload &payload)
{
}

void Processor::handle_write(RequestPayload &payload)
{
}


} // namespace postfga::bgw