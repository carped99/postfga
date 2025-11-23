#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "fga_type.h"

#ifdef __cplusplus
}
#endif

#include <variant>
#include <cstdlib>

namespace postfga::client {
    
struct CheckTupleRequest {
    const FgaRequest& in;

    uint32_t request_id() const noexcept { return in.request_id; }
    const FgaCheckTupleRequest& payload() const noexcept { return in.body.checkTuple; }
};

struct WriteTupleRequest {
    const FgaRequest& in;
    uint32_t request_id() const noexcept { return in.request_id; }
    const FgaWriteTupleRequest& payload() const noexcept { return in.body.writeTuple; }
};

struct DeleteTupleRequest {
    const FgaRequest& in;

    uint32_t request_id() const noexcept { return in.request_id; }
    const FgaDeleteTupleRequest& payload() const noexcept { return in.body.deleteTuple; }
};

struct GetStoreRequest {
    const FgaRequest& in;

    uint32_t request_id() const noexcept { return in.request_id; }
    const FgaGetStoreRequest& payload() const noexcept { return in.body.getStore; }
};

struct CreateStoreRequest {
    const FgaRequest& in;

    uint32_t request_id() const noexcept { return in.request_id; }
    const FgaCreateStoreRequest& payload() const noexcept { return in.body.createStore; }
};

using RequestVariant = std::variant<
    CheckTupleRequest,
    WriteTupleRequest,
    DeleteTupleRequest,
    GetStoreRequest,
    CreateStoreRequest
>;

inline RequestVariant make_request_variant(const FgaRequest& req)
{
    switch (static_cast<FgaRequestType>(req.type))
    {
        case FGA_REQ_CHECK:
            return CheckTupleRequest{req};
        case FGA_REQ_WRITE:
            return WriteTupleRequest{req};
        case FGA_REQ_DELETE:
            return DeleteTupleRequest{req};
        case FGA_REQ_GET_STORE:
            return GetStoreRequest{req};
        case FGA_REQ_CREATE_STORE:
            return CreateStoreRequest{req};
        default:
            // 여기서 에러 로그 찍거나, 예외 던지거나, std::abort 등 선택
            std::abort();
    }
}

} // namespace postfga::client