// openfga.hpp
#pragma once

#include <functional>

struct FgaRequest;
struct FgaResponse;

namespace postfga::client
{

    class Client
    {
      public:
        using ProcessCallback = std::function<void()>;

        virtual ~Client() = default;

        virtual bool is_healthy() const = 0;

        virtual void process(const FgaRequest& req, FgaResponse& res, ProcessCallback cb) = 0;

        virtual void shutdown() = 0;
    };

} // namespace postfga::client
