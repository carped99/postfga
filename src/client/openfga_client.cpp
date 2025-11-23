// openfga.cpp

#include <algorithm>
#include <cstring>
#include <thread>

#include "channel_factory.hpp"
#include "openfga_client.hpp"

namespace postfga::client {

/* ========================================================================
 * ctor / dtor
 * ====================================================================== */
OpenFgaGrpcClient::OpenFgaGrpcClient(const Config& config)
    : config_(config)
    , pool_(config.concurrency.worker_threads)
    , channel_(make_channel(config_))
    , handler_(std::make_unique<OpenFgaHandler>(config_, channel_))
    , inflight_(1000)  // 기본값, 필요시 설정 가능
{}

OpenFgaGrpcClient::~OpenFgaGrpcClient()
{
    shutdown();
}

/* ========================================================================
 * Public API
 * ====================================================================== */
bool OpenFgaGrpcClient::is_healthy() const
{
    if (!channel_ || stopping_.load(std::memory_order_acquire)) {
        return false;
    }
    
    // 채널 상태를 보고 대략적인 health 판단
    // true 를 넣으면, 필요 시 CONNECTING으로 전환을 트리거할 수 있음
    auto state = channel_->GetState(false /* try_to_connect */);
    switch (state) {
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

void OpenFgaGrpcClient::process(const FgaRequest& req, FgaResponseHandler handler)
{
    if (stopping_.load(std::memory_order_relaxed))
    {
        FgaResponse resp{};
        // resp.error = FgaError::Shutdown;
        //resp.error_message = "OpenFgaGrpcClient is shutting down";
        handler(std::move(resp));
        return;
    }

    auto guard = inflight_.acquire();
    if (!guard) {
        // 너무 많은 요청이 들어왔음
        FgaResponse resp{};
        // resp.status_code = 429; // Too Many Requests
        handler(std::move(resp));
        return;
    }

    auto self = shared_from_this();  // this 생존 보장
    asio::post(pool_, [self, req, cb = std::move(handler)]() mutable {
        FgaResponse resp;
        try {
            resp = self->handler_->handle(req);
        } catch (const std::exception& e) {
            // resp.error = FgaError::Internal;         // enum 가정
            // resp.error_message = e.what();
        } catch (...) {
            // resp.error = FgaError::Internal;
            // resp.error_message = "Unknown exception in OpenFgaHandler::handle()";
        }
        // callback
        cb(std::move(resp));
    });        
}

void OpenFgaGrpcClient::shutdown()
{
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        // 이미 shutdown 호출됨
        return;
    }

    // 새로운 작업이 더 이상 들어가지 않도록 되어 있으니,
    // 남아 있는 작업이 끝나길 기다린 후 thread_pool 종료
    // 필요하다면 inflight_ 0까지 기다리는 로직 추가 가능
    // ex) inflight_.wait_until_zero(timeout) 같은 메서드가 있다면 사용
    pool_.join();
}

// /* ========================================================================
//  * 내부: retry 래핑
//  * ====================================================================== */

//  FgaResponse OpenFgaGrpcClient::handle_request(const CheckTupleRequest& req) const{
//     FgaResponse out{};

//     auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms);
//     grpc::ClientContext ctx;
//     ctx.set_deadline(deadline);

//     auto greq = make_check_request(req);
//     openfga::v1::CheckResponse gresp;
//     grpc::Status status = stub_->Check(&ctx, greq, &gresp);
//     if (status.ok())
//     {
//         out.allowed     = gresp.allowed() ? 1 : 0;
//     } else {
//         out.status_code = static_cast<int>(status.error_code());
//         std::strncpy(out.error_message, status.error_message().c_str(), sizeof(out.error_message) - 1);
//     }
//     // out.status_code = 0;
//     return out;    
//  }



// CheckResponse OpenFgaGrpcClient::do_check_with_retry(const FgaRequest& req) const
// {
//     CheckResponse resp{};
//     for (int attempt = 0;; ++attempt)
//     {
//         resp = do_check_once(req);
//         if (resp.status_code == 0 /* OK*/ ||
//             !should_retry(grpc::Status(static_cast<grpc::StatusCode>(resp.status_code), ""),
//                           attempt))
//         {
//             break;
//         }

//         int backoff = next_backoff_ms(attempt);
//         if (backoff > 0)
//             std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
//     }
//     return resp;
// }

// WriteResponse OpenFgaGrpcClient::do_write_with_retry(const FgaRequest& req) const
// {
//     WriteResponse resp{};
//     for (int attempt = 0;; ++attempt)
//     {
//         resp = do_write_once(req);
//         if (resp.status_code == 0 /* OK*/ ||
//             !should_retry(grpc::Status(static_cast<grpc::StatusCode>(resp.status_code), ""),
//                           attempt))
//         {
//             break;
//         }

//         int backoff = next_backoff_ms(attempt);
//         if (backoff > 0)
//             std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
//     }
//     return resp;
// }

/* ========================================================================
 * 내부: 단일 호출
 * ====================================================================== */

// FgaCheckTupleResponse OpenFgaGrpcClient::handle_check(const FgaCreateTupleRequest& req) const
// {
//     FgaCheckTupleResponse out{};
//     grpc::ClientContext ctx;
//     auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(config_.timeout_ms);
//     ctx.set_deadline(deadline);

//     openfga::v1::CheckRequest greq = make_check_request(req);
//     openfga::v1::CheckResponse gresp;
//     grpc::Status status = stub_->Check(&ctx, greq, &gresp);
//     if (!status.ok())
//     {
//         out.status_code = static_cast<int>(status.error_code());
//         std::strncpy(out.error_message, status.error_message().c_str(), sizeof(out.error_message) - 1);
//         return out;
//     }

//     // 실제 proto 필드 확인 필요 (일반적으로 allowed() bool)
//     out.allowed     = gresp.allowed() ? 1 : 0;
//     // out.status_code = 0;
//     return out;
// }

// FgaWriteTupleResponse OpenFgaGrpcClient::handle_write(const FgaWriteTupleRequest& req) const
// {
//     FgaWriteTupleResponse out{};
//     out.status_code   = 0;
//     // out.openfga_error_code = 0;
//     // out.success_count            = 0;
//     out.error_message[0]   = '\0';

//     grpc::ClientContext ctx;
//     openfga::v1::WriteRequest  greq = make_write_request(req);
//     openfga::v1::WriteResponse gresp;

//     auto deadline = std::chrono::system_clock::now() +
//                     std::chrono::milliseconds(config_.timeout_ms);
//     ctx.set_deadline(deadline);

//     grpc::Status status = stub_->Write(&ctx, greq, &gresp);
//     if (!status.ok())
//     {
//         out.status_code = static_cast<int>(status.error_code());
//         std::strncpy(out.error_message,
//                      status.error_message().c_str(),
//                      sizeof(out.error_message) - 1);
//         return out;
//     }

//     // out.success_count          = 1;
//     // out.status_code = 0;
//     return out;
// }

/* ========================================================================
 * retry/backoff
 * ====================================================================== */

// bool OpenFgaGrpcClient::should_retry(const grpc::Status& status, int attempt) const
// {
//     if (status.ok())
//         return false;

//     if (attempt >= config_.max_retries)
//         return false;

//     using grpc::StatusCode;
//     switch (status.error_code())
//     {
//         case StatusCode::UNAVAILABLE:
//         case StatusCode::DEADLINE_EXCEEDED:
//         case StatusCode::ABORTED:
//         case StatusCode::INTERNAL:
//             return true;
//         default:
//             return false;
//     }
// }

// int OpenFgaGrpcClient::next_backoff_ms(int attempt) const
// {
//     if (attempt <= 0)
//         return 0;

//     int backoff = config_.initial_backoff_ms * (1 << (attempt - 1));
//     backoff = std::min(backoff, config_.max_backoff_ms);
//     return backoff;
// }

/* ========================================================================
 * proto 매핑
 * ====================================================================== */


} // namespace postfga::client