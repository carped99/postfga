// openfga.hpp
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <semaphore>
#include <string>

// gRPC / OpenFGA proto
#include <grpcpp/grpcpp.h>

#include "client.hpp"
#include "config/config.hpp"
#include "openfga/v1/openfga_service.grpc.pb.h"
#include "request_variant.hpp"
#include "util/counter.hpp"

namespace postfga::client
{

    class OpenFgaGrpcClient : public Client, public std::enable_shared_from_this<OpenFgaGrpcClient>
    {
      public:
        explicit OpenFgaGrpcClient(const Config& config);
        ~OpenFgaGrpcClient();

        bool is_healthy() const;

        void process(const FgaRequest& req, FgaResponse& res, FgaResponseHandler handler, void* ctx) override;

        void shutdown() override;

      private:
        void handle_request(const CheckTupleRequest& req, FgaResponse& res, FgaResponseHandler handler, void* ctx);
        void handle_request(const WriteTupleRequest& req, FgaResponse& res, FgaResponseHandler handler, void* ctx);
        void handle_request(const DeleteTupleRequest& req, FgaResponse& res, FgaResponseHandler handler, void* ctx);
        void handle_request(const GetStoreRequest& req, FgaResponse& res, FgaResponseHandler handler, void* ctx);
        void handle_request(const CreateStoreRequest& req, FgaResponse& res, FgaResponseHandler handler, void* ctx);

        Config config_;
        std::shared_ptr<::grpc::Channel> channel_;
        std::unique_ptr<openfga::v1::OpenFGAService::Stub> stub_;
        mutable std::mutex mu_;
        std::atomic<bool> stopping_{false};
        postfga::util::Counter inflight_;
    };

} // namespace postfga::client
