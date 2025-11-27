// openfga.hpp
#pragma once

struct FgaRequest;
struct FgaResponse;

namespace postfga::client
{

    class Client
    {
      public:
        using FgaResponseHandler = void (*)(const FgaResponse&, void* ctx) noexcept;

        virtual ~Client() = default;

        virtual bool is_healthy() const = 0;

        virtual void process(const FgaRequest& req, FgaResponse& res, FgaResponseHandler handler, void* ctx) = 0;

        virtual void shutdown() = 0;
    };

} // namespace postfga::client
