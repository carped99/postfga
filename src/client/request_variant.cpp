
#include "client/request_variant.hpp"

#include <stdexcept>
#include <string>

#include "request.h" // FgaRequest, FgaRequestType, FGA_REQ_* 실제 정의

namespace postfga::client
{
    std::uint32_t CheckTupleRequest::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaCheckTupleRequest& CheckTupleRequest::payload() const noexcept
    {
        return in.body.checkTuple;
    }

    std::uint32_t WriteTupleRequest::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaWriteTupleRequest& WriteTupleRequest::payload() const noexcept
    {
        return in.body.writeTuple;
    }

    std::uint32_t DeleteTupleRequest::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaDeleteTupleRequest& DeleteTupleRequest::payload() const noexcept
    {
        return in.body.deleteTuple;
    }

    std::uint32_t GetStoreRequest::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaGetStoreRequest& GetStoreRequest::payload() const noexcept
    {
        return in.body.getStore;
    }

    std::uint32_t CreateStoreRequest::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaCreateStoreRequest& CreateStoreRequest::payload() const noexcept
    {
        return in.body.createStore;
    }

    // ----- variant factory 구현 -----

    RequestVariant make_request_variant(const FgaRequest& req)
    {
        switch (static_cast<FgaRequestType>(req.type))
        {
        case FGA_REQUEST_CHECK_TUPLE:
            return CheckTupleRequest{req};
        case FGA_REQUEST_WRITE_TUPLE:
            return WriteTupleRequest{req};
        case FGA_REQUEST_DELETE_TUPLE:
            return DeleteTupleRequest{req};
        case FGA_REQUEST_GET_STORE:
            return GetStoreRequest{req};
        case FGA_REQUEST_CREATE_STORE:
            return CreateStoreRequest{req};
        }

        throw std::logic_error("unknown FgaRequest type: " + std::to_string(req.type));
    }

} // namespace postfga::client
