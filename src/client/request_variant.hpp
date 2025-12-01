#pragma once

#include <cstdint>
#include <variant>

// forward declaration
struct FgaRequest;
struct FgaResponse;
struct FgaCheckTupleRequest;
struct FgaCheckTupleResponse;
struct FgaWriteTupleRequest;
struct FgaWriteTupleResponse;
struct FgaDeleteTupleRequest;
struct FgaDeleteTupleResponse;
struct FgaGetStoreRequest;
struct FgaGetStoreResponse;
struct FgaCreateStoreRequest;
struct FgaCreateStoreResponse;

namespace postfga::client
{
    struct InvalidRequest
    {
        const FgaRequest& in;
        FgaResponse& out;
    };

    struct CheckTuple
    {
        const FgaRequest& in;
        FgaResponse& out;

        std::uint32_t request_id() const noexcept;
        const FgaCheckTupleRequest& payload() const noexcept;
        FgaResponse& response() const noexcept;
    };

    struct WriteTuple
    {
        const FgaRequest& in;
        FgaResponse& out;

        std::uint32_t request_id() const noexcept;
        const FgaWriteTupleRequest& payload() const noexcept;
        FgaResponse& response() const noexcept;
    };

    struct DeleteTuple
    {
        const FgaRequest& in;
        FgaResponse& out;

        std::uint32_t request_id() const noexcept;
        const FgaDeleteTupleRequest& payload() const noexcept;
        FgaResponse& response() const noexcept;
    };

    struct GetStoreRequest
    {
        const FgaRequest& in;
        FgaResponse& out;

        std::uint32_t request_id() const noexcept;
        const FgaGetStoreRequest& payload() const noexcept;
        FgaResponse& response() const noexcept;
    };

    struct CreateStoreRequest
    {
        const FgaRequest& in;
        FgaResponse& out;

        std::uint32_t request_id() const noexcept;
        const FgaCreateStoreRequest& payload() const noexcept;
        FgaResponse& response() const noexcept;
    };

    using RequestVariant = std::variant<
        CheckTuple, 
        WriteTuple, 
        DeleteTuple, 
        GetStoreRequest, 
        CreateStoreRequest, 
        InvalidRequest>;

    // 구현은 .cpp 에서
    RequestVariant make_request_variant(const FgaRequest& req, FgaResponse& res);

} // namespace postfga::client
