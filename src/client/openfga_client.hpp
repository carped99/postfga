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

namespace fga::client
{
    struct BatchCheckItem
    {
        CheckTuple params;
        ProcessCallback callback;
    };

    class OpenFgaGrpcClient : public Client, public std::enable_shared_from_this<OpenFgaGrpcClient>
    {
      public:
        explicit OpenFgaGrpcClient(const fga::Config& config);
        ~OpenFgaGrpcClient();

        bool is_healthy() const;

        void process(FgaPayload& payload, ProcessCallback cb) override;
        void process_batch(std::span<ProcessItem> items) override;

        void shutdown() override;

      private:
        void handle_check_batch(std::vector<BatchCheckItem> items);
        void handle_request(CheckTuple& req, ProcessCallback cb);
        void handle_request(WriteTuple& req, ProcessCallback cb);
        void handle_request(DeleteTuple& req, ProcessCallback cb);
        void handle_request(GetStore& req, ProcessCallback cb);
        void handle_request(CreateStore& req, ProcessCallback cb);
        void handle_request(DeleteStore& req, ProcessCallback cb);
        void handle_request(InvalidRequest& req, ProcessCallback cb);

        fga::Config config_;
        std::shared_ptr<::grpc::Channel> channel_;
        std::unique_ptr<openfga::v1::OpenFGAService::Stub> stub_;
        mutable std::mutex mu_;
        std::atomic<bool> stopping_{false};
        fga::util::Counter inflight_;
    };

} // namespace fga::client
