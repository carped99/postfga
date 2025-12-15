#pragma once

#include <cstdint>
#include <variant>

#include "payload.h"

namespace fga::client
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
        const char* store_id() const noexcept;
        const char* model_id() const noexcept;
        const FgaCheckTupleRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline const char* CheckTuple::store_id() const noexcept
    {
        return payload.request.store_id;
    }

    inline const char* CheckTuple::model_id() const noexcept
    {
        return payload.request.model_id;
    }

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
        const char* store_id() const noexcept;
        const char* model_id() const noexcept;
        uint64_t request_id() const noexcept;
        const FgaWriteTupleRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline const char* WriteTuple::store_id() const noexcept
    {
        return payload.request.store_id;
    }

    inline const char* WriteTuple::model_id() const noexcept
    {
        return payload.request.model_id;
    }

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
        const char* store_id() const noexcept;
        const char* model_id() const noexcept;
        uint64_t request_id() const noexcept;
        const FgaDeleteTupleRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline const char* DeleteTuple::store_id() const noexcept
    {
        return payload.request.store_id;
    }

    inline const char* DeleteTuple::model_id() const noexcept
    {
        return payload.request.model_id;
    }

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

    struct GetStore
    {
        FgaPayload& payload;
        const char* store_id() const noexcept;
        uint64_t request_id() const noexcept;
        const FgaGetStoreRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline const char* GetStore::store_id() const noexcept
    {
        return payload.request.store_id;
    }

    inline uint64_t GetStore::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    inline const FgaGetStoreRequest& GetStore::request() const noexcept
    {
        return payload.request.body.getStore;
    }

    inline FgaResponse& GetStore::response() const noexcept
    {
        return payload.response;
    }

    struct CreateStore
    {
        FgaPayload& payload;
        uint64_t request_id() const noexcept;
        const FgaCreateStoreRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline uint64_t CreateStore::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    inline const FgaCreateStoreRequest& CreateStore::request() const noexcept
    {
        return payload.request.body.createStore;
    }

    inline FgaResponse& CreateStore::response() const noexcept
    {
        return payload.response;
    }

    struct DeleteStore
    {
        FgaPayload& payload;
        const char* store_id() const noexcept;
        uint64_t request_id() const noexcept;
        const FgaDeleteStoreRequest& request() const noexcept;
        FgaResponse& response() const noexcept;
    };

    inline const char* DeleteStore::store_id() const noexcept
    {
        return payload.request.store_id;
    }

    inline uint64_t DeleteStore::request_id() const noexcept
    {
        return payload.request.request_id;
    }

    inline const FgaDeleteStoreRequest& DeleteStore::request() const noexcept
    {
        return payload.request.body.deleteStore;
    }

    inline FgaResponse& DeleteStore::response() const noexcept
    {
        return payload.response;
    }

    using RequestVariant =
        std::variant<CheckTuple, WriteTuple, DeleteTuple, GetStore, CreateStore, DeleteStore, InvalidRequest>;

    inline RequestVariant make_request_variant(FgaPayload& payload)
    {
        switch (static_cast<FgaRequestType>(payload.request.type))
        {
        case FGA_REQUEST_CHECK:
            return CheckTuple{payload};
        case FGA_REQUEST_WRITE_TUPLE:
            return WriteTuple{payload};
        case FGA_REQUEST_DELETE_TUPLE:
            return DeleteTuple{payload};
        case FGA_REQUEST_GET_STORE:
            return GetStore{payload};
        case FGA_REQUEST_CREATE_STORE:
            return CreateStore{payload};
        case FGA_REQUEST_DELETE_STORE:
            return DeleteStore{payload};
        default:
            return InvalidRequest{payload};
        }
    }
} // namespace fga::client
