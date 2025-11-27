// openfga.cpp

#include "openfga_client.hpp"

#include <algorithm>
#include <cstring>
#include <thread>

#include "channel_factory.hpp"
#include "util/logger.hpp"

namespace postfga::client
{


    /* ========================================================================
     * ctor / dtor
     * ====================================================================== */
    OpenFgaGrpcClient::OpenFgaGrpcClient(const Config& config)
        : config_(config),
          pool_(config.concurrency.worker_threads),
          channel_(make_channel(config_)),
          stub_(openfga::v1::OpenFGAService::NewStub(channel_)),
          inflight_(1000) // 기본값, 필요시 설정 가능
    {
    }

    OpenFgaGrpcClient::~OpenFgaGrpcClient()
    {
        shutdown();
    }

    /* ========================================================================
     * Public API
     * ====================================================================== */
    bool OpenFgaGrpcClient::is_healthy() const
    {
        if (!channel_ || stopping_.load(std::memory_order_acquire))
        {
            return false;
        }

        // 채널 상태를 보고 대략적인 health 판단
        // true 를 넣으면, 필요 시 CONNECTING으로 전환을 트리거할 수 있음
        auto state = channel_->GetState(false /* try_to_connect */);
        switch (state)
        {
        case GRPC_CHANNEL_READY:
            return true;

        case GRPC_CHANNEL_IDLE:
        case GRPC_CHANNEL_CONNECTING:
            // 상황에 따라 true/false 선택 (여기선 "아직 완전 실패는 아님"으로 취급)
            return true;

        case GRPC_CHANNEL_TRANSIENT_FAILURE:
        case GRPC_CHANNEL_SHUTDOWN:
        default:
            return false;
        }
        // auto deadline = std::chrono::system_clock::now() +
        //                 std::chrono::milliseconds(config_.timeout_ms);
        // return channel_->WaitForConnected(deadline);
    }

    void OpenFgaGrpcClient::process(const FgaRequest& req, FgaResponse& res, FgaResponseHandler handler, void* ctx)
    {
        // if (stopping_.load(std::memory_order_relaxed))
        // {
        //     FgaResponse resp{};
        //     // resp.error = FgaError::Shutdown;
        //     // resp.error_message = "OpenFgaGrpcClient is shutting down";
        //     handler(std::move(resp));
        //     return;
        // }

        // auto guard = inflight_.acquire();
        // if (!guard)
        // {
        //     // 너무 많은 요청이 들어왔음
        //     FgaResponse resp{};
        //     // resp.status_code = 429; // Too Many Requests
        //     handler(std::move(resp), ctx);
        //     return;
        // }

        auto variant = make_request_variant(req);
        std::visit([this, &res, handler, ctx](const auto& arg) { this->handle_request(arg, res, handler, ctx); },
                   variant);
    }

    void OpenFgaGrpcClient::shutdown()
    {
        bool expected = false;
        if (!stopping_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            // 이미 shutdown 호출됨
            return;
        }

        // 새로운 작업이 더 이상 들어가지 않도록 되어 있으니,
        // 남아 있는 작업이 끝나길 기다린 후 thread_pool 종료
        // 필요하다면 inflight_ 0까지 기다리는 로직 추가 가능
        // ex) inflight_.wait_until_zero(timeout) 같은 메서드가 있다면 사용
        pool_.join();
    }
} // namespace postfga::client
