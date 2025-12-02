#pragma once

#include <cstdint>
#include <variant>

#include "payload.h"

namespace postfga::client
{
    struct InvalidRequest
    {
        FgaPayload& payload;
        uint64_t request_id() const noexcept;
    };

    inline uint64_t InvalidRequest::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    struct CheckTuple
    {
        FgaPayload& payload;
        uint64_t request_id() const noexcept;
        const FgaCheckTupleRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline uint64_t CheckTuple::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    inline const FgaCheckTupleRequest& CheckTuple::request() const noexcept
    {
        return payload.request.body.checkTuple;
    }

    inline FgaResponse& CheckTuple::response() const noexcept
    {
        return payload.response;
    }

    struct WriteTuple
    {
        FgaPayload& payload;
        uint64_t request_id() const noexcept;
        const FgaWriteTupleRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline uint64_t WriteTuple::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    inline const FgaWriteTupleRequest& WriteTuple::request() const noexcept
    {
        return payload.request.body.writeTuple;
    }

    inline FgaResponse& WriteTuple::response() const noexcept
    {
        return payload.response;
    }

    struct DeleteTuple
    {
        FgaPayload& payload;
        uint64_t request_id() const noexcept;
        const FgaDeleteTupleRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline uint64_t DeleteTuple::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    inline const FgaDeleteTupleRequest& DeleteTuple::request() const noexcept
    {
        return payload.request.body.deleteTuple;
    }

    inline FgaResponse& DeleteTuple::response() const noexcept
    {
        return payload.response;
    }

    struct GetStoreRequest
    {
        FgaPayload& payload;
        uint64_t request_id() const noexcept;
        const FgaGetStoreRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline uint64_t GetStoreRequest::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    inline const FgaGetStoreRequest& GetStoreRequest::request() const noexcept
    {
        return payload.request.body.getStore;
    }

    inline FgaResponse& GetStoreRequest::response() const noexcept
    {
        return payload.response;
    }

    struct CreateStoreRequest
    {
        FgaPayload& payload;
        uint64_t request_id() const noexcept;
        const FgaCreateStoreRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline uint64_t CreateStoreRequest::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    inline const FgaCreateStoreRequest& CreateStoreRequest::request() const noexcept
    {
        return payload.request.body.createStore;
    }

    inline FgaResponse& CreateStoreRequest::response() const noexcept
    {
        return payload.response;
    }

    using RequestVariant = std::variant<
        CheckTuple, 
        WriteTuple, 
        DeleteTuple, 
        GetStoreRequest, 
        CreateStoreRequest, 
        InvalidRequest>;

    inline RequestVariant make_request_variant(FgaPayload& payload)
    {
        switch (static_cast<FgaRequestType>(payload.request.type))
        {
        case FGA_REQUEST_CHECK_TUPLE:
            return CheckTuple{payload};
        case FGA_REQUEST_WRITE_TUPLE:
            return WriteTuple{payload};
        case FGA_REQUEST_DELETE_TUPLE:
            return DeleteTuple{payload};
        case FGA_REQUEST_GET_STORE:
            return GetStoreRequest{payload};
        case FGA_REQUEST_CREATE_STORE:
            return CreateStoreRequest{payload};
        default:
            return InvalidRequest{payload};
        }
    }
} // namespace postfga::client
