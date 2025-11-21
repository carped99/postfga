// client_adapter.hpp
#pragma once

#include <memory>
#include "config.hpp"
#include "openfga.hpp"

namespace postfga::client {

class Client {
public:
    explicit Client(const Config& config);

    bool ensure_initialized();
    bool is_healthy() const;

    using CheckCallback = OpenFgaGrpcClient::CheckHandler;
    using WriteCallback = OpenFgaGrpcClient::WriteHandler;

    void check_async(const GrpcCheckRequest& req, CheckCallback handler);

    void write_async(const GrpcWriteRequest& req,WriteCallback handler);

    void shutdown();

private:
    Config                       config_;
    std::shared_ptr<OpenFgaGrpcClient> client_;
};

} // namespace postfga::client
