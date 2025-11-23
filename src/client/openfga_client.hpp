// openfga.hpp
#pragma once

#include <asio.hpp>

#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <chrono>
#include <semaphore>
#include <limits>

// gRPC / OpenFGA proto
#include <grpcpp/grpcpp.h>

#include "config/config.hpp"
#include "client.hpp"
#include "util/counter.hpp"
#include "openfga_handler.hpp"

namespace postfga::client
{

    class OpenFgaGrpcClient
        : public Client,
          public std::enable_shared_from_this<OpenFgaGrpcClient>
    {
    public:
        explicit OpenFgaGrpcClient(const Config &config);
        ~OpenFgaGrpcClient();

        bool is_healthy() const;

        void process(const FgaRequest &req, Client::FgaResponseHandler handler);

        void shutdown();

    private:
        Config config_;
        asio::thread_pool pool_;
        std::shared_ptr<grpc::Channel> channel_;
        std::unique_ptr<OpenFgaHandler> handler_;
        mutable std::mutex mu_;
        std::atomic<bool> stopping_{false};
        postfga::util::Counter inflight_;
    };

} // namespace postfga::client