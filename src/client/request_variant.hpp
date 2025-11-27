#pragma once

#include <cstdint>
#include <variant>

// forward declaration
struct FgaRequest;
struct FgaCheckTupleRequest;
struct FgaWriteTupleRequest;
struct FgaDeleteTupleRequest;
struct FgaGetStoreRequest;
struct FgaCreateStoreRequest;

namespace postfga::client
{
    struct CheckTupleRequest
    {
        const FgaRequest& in;

        std::uint32_t request_id() const noexcept;
        const FgaCheckTupleRequest& payload() const noexcept;
    };

    struct WriteTupleRequest
    {
        const FgaRequest& in;

        std::uint32_t request_id() const noexcept;
        const FgaWriteTupleRequest& payload() const noexcept;
    };

    struct DeleteTupleRequest
    {
        const FgaRequest& in;

        std::uint32_t request_id() const noexcept;
        const FgaDeleteTupleRequest& payload() const noexcept;
    };

    struct GetStoreRequest
    {
        const FgaRequest& in;

        std::uint32_t request_id() const noexcept;
        const FgaGetStoreRequest& payload() const noexcept;
    };

    struct CreateStoreRequest
    {
        const FgaRequest& in;

        std::uint32_t request_id() const noexcept;
        const FgaCreateStoreRequest& payload() const noexcept;
    };

    using RequestVariant =
        std::variant<CheckTupleRequest, WriteTupleRequest, DeleteTupleRequest, GetStoreRequest, CreateStoreRequest>;

    // 구현은 .cpp 에서
    RequestVariant make_request_variant(const FgaRequest& req);

} // namespace postfga::client
