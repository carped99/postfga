// openfga.hpp
#pragma once

#include <functional>
#include <memory>
#include <span>

#include "config/config.hpp"

struct FgaPayload;

namespace fga::client
{

    using ProcessCallback = std::function<void()>;

    struct ProcessItem
    {
        FgaPayload* payload;
        ProcessCallback callback;
    };

    class Client
    {
      public:
        virtual ~Client() = default;

        virtual bool is_healthy() const = 0;

        virtual void process(FgaPayload& payload, ProcessCallback cb) = 0;
        virtual void process_batch(std::span<ProcessItem> items) = 0;

        virtual void shutdown() = 0;
    };

    std::shared_ptr<Client> make_client(const fga::Config& cfg);
} // namespace fga::client
