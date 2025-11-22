// openfga.hpp
#pragma once

#include <asio.hpp>

#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <chrono>

// C struct forward 선언 (C linkage는 struct엔 필요 없음)
struct GrpcCheckRequest;
struct GrpcWriteRequest;

// gRPC / OpenFGA proto
#include <grpcpp/grpcpp.h>
#include <openfga/v1/openfga_service.grpc.pb.h>
#include <openfga/v1/openfga_service.pb.h>

#include "config.hpp"
#include "client.hpp"

namespace postfga::client {

class OpenFgaGrpcClient 
    : public Client
    , public std::enable_shared_from_this<OpenFgaGrpcClient> 
{
public:
    explicit OpenFgaGrpcClient(const Config& config);
    ~OpenFgaGrpcClient();

    // 채널 health check (sync)
    bool is_healthy() const;

    // 비동기 Check (standalone asio 사용)
    void async_check(const Request& req, Client::CheckHandler handler);

    // 비동기 Write
    void async_write(const Request& req, Client::WriteHandler handler);

    // graceful shutdown (선택적으로 호출)
    void shutdown();

private:
    // 내부 Sync 호출 (재시도 포함)
    CheckResponse  do_check_with_retry(const Request& req) const;
    WriteResponse  do_write_with_retry(const Request& req) const;

    CheckResponse  do_check_once(const Request& req) const;
    WriteResponse  do_write_once(const Request& req) const;

    bool should_retry(const grpc::Status& status, int attempt) const;
    int  next_backoff_ms(int attempt) const;

    // OpenFGA proto 매핑 헬퍼
    void fill_check_request(const Request& in,
                            openfga::v1::CheckRequest& out) const;
    void fill_write_request(const Request& in,
                            openfga::v1::WriteRequest& out) const;

    Config config_;

    std::shared_ptr<grpc::Channel>                        channel_;
    std::unique_ptr<openfga::v1::OpenFGAService::Stub>    stub_;

    asio::thread_pool                                     pool_;
    mutable std::mutex                                    mu_;
    std::atomic<bool>                                     stopping_{false};
    std::atomic<int>                                      inflight_{0};
};

} // namespace postfga::client
