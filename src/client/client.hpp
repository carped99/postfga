// openfga.hpp
#pragma once

#include <functional>
#include <memory>
#include "fga_type.h"
#include "response.h"

namespace postfga::client
{

    class Client
    {
    public:
        using FgaResponseHandler = std::function<void(const FgaResponse &)>;

        virtual ~Client() = default;

        virtual bool is_healthy() const = 0;

        virtual void process(const FgaRequest &req, FgaResponseHandler handler) = 0;

        virtual void shutdown() = 0;
    };

} // namespace postfga::client
