
#include "client/request_variant.hpp"

#include <stdexcept>
#include <string>

#include "request.h" // FgaRequest, FgaRequestType, FGA_REQ_* 실제 정의

namespace postfga::client
{
    std::uint32_t CheckTuple::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaCheckTupleRequest& CheckTuple::payload() const noexcept
    {
        return in.body.checkTuple;
    }

    FgaResponse& CheckTuple::response() const noexcept
    {
        return out;
    }

    std::uint32_t WriteTuple::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaWriteTupleRequest& WriteTuple::payload() const noexcept
    {
        return in.body.writeTuple;
    }

    FgaResponse& WriteTuple::response() const noexcept
    {
        return out;
    }

    std::uint32_t DeleteTuple::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaDeleteTupleRequest& DeleteTuple::payload() const noexcept
    {
        return in.body.deleteTuple;
    }

    FgaResponse& DeleteTuple::response() const noexcept
    {
        return out;
    }

    std::uint32_t GetStoreRequest::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaGetStoreRequest& GetStoreRequest::payload() const noexcept
    {
        return in.body.getStore;
    }

    FgaResponse& GetStoreRequest::response() const noexcept
    {
        return out;
    }

    std::uint32_t CreateStoreRequest::request_id() const noexcept
    {
        return in.request_id;
    }

    const FgaCreateStoreRequest& CreateStoreRequest::payload() const noexcept
    {
        return in.body.createStore;
    }

    FgaResponse& CreateStoreRequest::response() const noexcept
    {
        return out;
    }

    // ----- variant factory 구현 -----

    RequestVariant make_request_variant(const FgaRequest& req, FgaResponse& res)
    {
        switch (static_cast<FgaRequestType>(req.type))
        {
        case FGA_REQUEST_CHECK_TUPLE:
            return CheckTuple{req, res};
        case FGA_REQUEST_WRITE_TUPLE:
            return WriteTuple{req, res};
        case FGA_REQUEST_DELETE_TUPLE:
            return DeleteTuple{req, res};
        case FGA_REQUEST_GET_STORE:
            return GetStoreRequest{req, res};
        case FGA_REQUEST_CREATE_STORE:
            return CreateStoreRequest{req, res};
        default:
            return InvalidRequest{req, res};
        }
    }

} // namespace postfga::client
