// openfga.hpp
#pragma once

#include <functional>
#include <span>
#include <memory>

struct Config;
struct FgaPayload;

namespace postfga::client
{

    using ProcessCallback = std::function<void()>;

    struct ProcessItem
    {
        FgaPayload* payload;
        ProcessCallback   callback;
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
    
    std::shared_ptr<Client> make_client(const Config& cfg);
} // namespace postfga::client
