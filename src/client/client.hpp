// openfga.hpp
#pragma once

#include <functional>
#include <span>

struct FgaRequest;
struct FgaResponse;

namespace postfga::client
{

    using ProcessCallback = std::function<void()>;

    struct ProcessItem
    {
        const FgaRequest* request;
        FgaResponse*      response;
        ProcessCallback   callback;
    };

    class Client
    {
      public:
        

        virtual ~Client() = default;

        virtual bool is_healthy() const = 0;

        void process(const FgaRequest& req, FgaResponse& res, ProcessCallback cb){
            ProcessItem item { &req, &res, std::move(cb) };
            process_batch(std::span<ProcessItem>(&item, 1));
        }

        virtual void process_batch(std::span<ProcessItem> items) = 0;

        virtual void shutdown() = 0;
    };

} // namespace postfga::client
