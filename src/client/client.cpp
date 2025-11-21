// client.cpp
#include "client.hpp"

namespace postfga::client {

Client::Client(const Config& config)
  : config_(config)
  , client_(std::make_shared<OpenFgaGrpcClient>(config_))
{
}

bool Client::ensure_initialized()
{
    // 필요하면 여기서 GUC 다시 읽어서 config_ 갱신 + client_ 재생성
    return client_ && client_->is_healthy();
}

bool Client::is_healthy() const
{
    return client_ && client_->is_healthy();
}

void Client::check_async(const GrpcCheckRequest& req,
                                CheckCallback handler)
{
    if (client_) {
        client_->async_check(req, handler);
    }
    // 필요하면 nullptr / unhealthy 에 대한 fallback 처리 추가
}

void Client::write_async(const GrpcWriteRequest& req,
                                WriteCallback handler)
{
    if (client_) {
        client_->async_write(req, handler);
    }
}

void Client::shutdown()
{
    if (client_) {
        client_->shutdown();
    }
}

} // namespace postfga::client
