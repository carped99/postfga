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
#include "request_variant.hpp"
#include "openfga/v1/openfga_service.grpc.pb.h"

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

        void process(const FgaRequest &req, FgaResponseHandler handler, void *ctx) override;

        void shutdown() override;

    private:
        void handle_request(const CheckTupleRequest &req, FgaResponseHandler handler, void *ctx);
        void handle_request(const WriteTupleRequest &req, FgaResponseHandler handler, void *ctx);
        void handle_request(const DeleteTupleRequest &req, FgaResponseHandler handler, void *ctx);
        void handle_request(const GetStoreRequest &req, FgaResponseHandler handler, void *ctx);
        void handle_request(const CreateStoreRequest &req, FgaResponseHandler handler, void *ctx);

        Config config_;
        asio::thread_pool pool_;
        std::shared_ptr<::grpc::Channel> channel_;
        std::unique_ptr<openfga::v1::OpenFGAService::Stub> stub_;
        mutable std::mutex mu_;
        std::atomic<bool> stopping_{false};
        postfga::util::Counter inflight_;
    };

} // namespace postfga::client