/*
 * client.cpp
 *
 * OpenFGA gRPC async client implementation using gRPC CompletionQueue.
 *
 * IMPORTANT: This file does NOT include PostgreSQL headers to avoid macro conflicts.
 * All PostgreSQL-specific types are passed through the C interface in client.h.
 */

#include "client.h"

#include <string.h>
#include <stdio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

// gRPC and OpenFGA proto
#include <grpcpp/grpcpp.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include <openfga/v1/openfga_service.grpc.pb.h>
#include <openfga/v1/openfga_service.pb.h>

/* ========================================================================
 * Constants & helpers
 * ======================================================================== */

namespace {

constexpr int      kDefaultTimeoutSeconds = 5;
constexpr int      kMaxGrpcMessageSize    = 4 * 1024 * 1024;  // 4MB
constexpr uint32_t kInternalErrorCode     = 1000;

inline bool cstr_not_empty(const char *s)
{
    return s && s[0] != '\0';
}

inline std::string join_type_id(const char *type, const char *id)
{
    std::string result;
    const auto len_type = std::strlen(type);
    const auto len_id   = std::strlen(id);
    result.reserve(len_type + 1 + len_id);
    result.append(type, len_type);
    result.push_back(':');
    result.append(id, len_id);
    return result;
}

struct AsyncCheckCall {
    grpc::ClientContext context;
    CheckCallback       callback = nullptr;
    void               *user_data = nullptr;

    openfga::v1::CheckRequest  grpc_request;
    openfga::v1::CheckResponse grpc_response;
    grpc::Status               status;

    std::unique_ptr<grpc::ClientAsyncResponseReader<openfga::v1::CheckResponse>> rpc;
};

/* 요청 → gRPC 메시지 변환 */
void fill_grpc_check_request(const GrpcCheckRequest *req,
                             openfga::v1::CheckRequest *grpc_req)
{
    // store_id
    if (cstr_not_empty(req->store_id))
        grpc_req->set_store_id(req->store_id);

    // authorization_model_id
    if (cstr_not_empty(req->authorization_model_id))
        grpc_req->set_authorization_model_id(req->authorization_model_id);

    auto *tuple_key = grpc_req->mutable_tuple_key();

    // object: "type:id"
    if (cstr_not_empty(req->object_type) && cstr_not_empty(req->object_id))
        tuple_key->set_object(join_type_id(req->object_type, req->object_id));

    // relation
    if (cstr_not_empty(req->relation))
        tuple_key->set_relation(req->relation);

    // user: "type:id"
    if (cstr_not_empty(req->subject_type) && cstr_not_empty(req->subject_id))
        tuple_key->set_user(join_type_id(req->subject_type, req->subject_id));
}

/* gRPC 응답/Status → CheckResponse 변환 */
void fill_check_response(const openfga::v1::CheckResponse *grpc_resp,
                         const grpc::Status &status,
                         bool ok_flag,
                         CheckResponse *resp)
{
    // 기본값 초기화
    resp->allowed = false;
    resp->error_code = 0;
    resp->error_message[0] = '\0';

    // CompletionQueue에서 ok=false 는 스트림/컨텍스트 에러 상황으로 간주
    if (!ok_flag) {
        resp->allowed = false;
        resp->error_code = kInternalErrorCode;
        std::snprintf(resp->error_message, sizeof(resp->error_message),
                      "gRPC completion queue reported failure");
        return;
    }

    if (status.ok()) {
        resp->allowed = grpc_resp ? grpc_resp->allowed() : false;
        return;
    }

    resp->allowed = false;
    resp->error_code = static_cast<uint32_t>(status.error_code());

    const std::string &msg = status.error_message();
    const size_t copy_len =
        std::min(msg.size(), sizeof(resp->error_message) - 1);

    if (copy_len > 0)
        std::memcpy(resp->error_message, msg.data(), copy_len);

    resp->error_message[copy_len] = '\0';
}

/* ========================================================================
 * GrpcClientImpl: 실제 C++ 구현 클래스
 * ======================================================================== */

class GrpcClientImpl {
public:
    explicit GrpcClientImpl(const std::string &endpoint)
        : running_(false),
          total_requests_(0),
          total_errors_(0)
    {
        grpc::ChannelArguments args;
        args.SetMaxReceiveMessageSize(kMaxGrpcMessageSize);
        args.SetMaxSendMessageSize(kMaxGrpcMessageSize);

        channel_ = grpc::CreateCustomChannel(
            endpoint,
            grpc::InsecureChannelCredentials(),
            args);

        if (!channel_) {
            throw std::runtime_error("Failed to create gRPC channel");
        }

        stub_ = openfga::v1::OpenFGAService::NewStub(channel_);
        cq_   = std::make_unique<grpc::CompletionQueue>();

        running_ = true;
        cq_thread_ = std::thread(&GrpcClientImpl::RunCompletionQueue, this);
    }

    ~GrpcClientImpl()
    {
        Shutdown();
    }

    bool CheckSync(const GrpcCheckRequest &request, CheckResponse &response)
    {
        try {
            grpc::ClientContext context;
            context.set_deadline(std::chrono::system_clock::now() +
                                 std::chrono::seconds(kDefaultTimeoutSeconds));

            openfga::v1::CheckRequest  grpc_req;
            openfga::v1::CheckResponse grpc_resp;

            fill_grpc_check_request(&request, &grpc_req);

            grpc::Status status = stub_->Check(&context, grpc_req, &grpc_resp);

            fill_check_response(&grpc_resp, status, /*ok_flag*/true, &response);

            total_requests_.fetch_add(1, std::memory_order_relaxed);
            if (!status.ok())
                total_errors_.fetch_add(1, std::memory_order_relaxed);

            return status.ok();
        } catch (const std::exception &e) {
            response.allowed = false;
            response.error_code = kInternalErrorCode;
            std::snprintf(response.error_message, sizeof(response.error_message),
                          "Exception: %s", e.what());
            total_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    bool CheckAsync(const GrpcCheckRequest &request,
                    CheckCallback callback,
                    void *user_data)
    {
        if (!callback)
            return false;

        try {
            auto *call = new AsyncCheckCall();

            call->callback  = callback;
            call->user_data = user_data;

            // timeout 설정
            call->context.set_deadline(
                std::chrono::system_clock::now() +
                std::chrono::seconds(kDefaultTimeoutSeconds));

            // gRPC 요청 메시지 채우기
            fill_grpc_check_request(&request, &call->grpc_request);

            // 비동기 호출 시작
            call->rpc = stub_->AsyncCheck(
                &call->context,
                call->grpc_request,
                cq_.get());

            // 완료 시점에 큐로 tag 전달
            call->rpc->Finish(&call->grpc_response,
                              &call->status,
                              static_cast<void*>(call));

            total_requests_.fetch_add(1, std::memory_order_relaxed);
            return true;
        } catch (const std::exception &) {
            total_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    bool IsHealthy() const
    {
        if (!channel_)
            return false;

        try {
            auto state = channel_->GetState(false);
            return (state == GRPC_CHANNEL_READY ||
                    state == GRPC_CHANNEL_IDLE);
        } catch (...) {
            return false;
        }
    }

    void Shutdown()
    {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false))
            return; // 이미 정리됨

        if (cq_) {
            cq_->Shutdown();
        }

        if (cq_thread_.joinable()) {
            cq_thread_.join();
        }
    }

private:
    void RunCompletionQueue()
    {
        void *tag = nullptr;
        bool ok   = false;

        while (running_) {
            const auto deadline =
                std::chrono::system_clock::now() +
                std::chrono::milliseconds(100);

            auto result = cq_->AsyncNext(&tag, &ok, deadline);

            if (result == grpc::CompletionQueue::GOT_EVENT) {
                auto *call = static_cast<AsyncCheckCall*>(tag);
                if (call) {
                    CheckResponse resp;
                    fill_check_response(&call->grpc_response,
                                        call->status,
                                        ok,
                                        &resp);

                    if (call->callback) {
                        // 콜백 안에서 예외 터져도 여기까지 전파되지 않게 방어
                        try {
                            call->callback(&resp, call->user_data);
                        } catch (...) {
                            // PostgreSQL 로 예외 넘기지 않기 위해 삼켜버림
                        }
                    }
                    delete call;
                }
            } else if (result == grpc::CompletionQueue::TIMEOUT) {
                // 그냥 다시 루프
                continue;
            } else { // SHUTDOWN
                break;
            }
        }

        // Shutdown 이후 남은 이벤트가 있다면 모두 drain
        void *drain_tag = nullptr;
        bool drain_ok   = false;
        while (cq_->Next(&drain_tag, &drain_ok)) {
            auto *call = static_cast<AsyncCheckCall*>(drain_tag);
            if (call) {
                CheckResponse resp;
                fill_check_response(&call->grpc_response,
                                    call->status,
                                    drain_ok,
                                    &resp);
                if (call->callback) {
                    try {
                        call->callback(&resp, call->user_data);
                    } catch (...) {
                        // 무시
                    }
                }
                delete call;
            }
        }
    }

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<openfga::v1::OpenFGAService::Stub> stub_;
    std::unique_ptr<grpc::CompletionQueue> cq_;

    std::thread      cq_thread_;
    std::atomic<bool> running_;

    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> total_errors_;
};

} // namespace

/* ========================================================================
 * C API  
 * ======================================================================== */
struct GrpcClient {
    std::unique_ptr<GrpcClientImpl> impl;
};

extern "C" {
GrpcClient *postfga_client_init(const char *endpoint)
{
    if (!endpoint || !endpoint[0])
        return nullptr;

    try {
        auto wrapper = std::make_unique<GrpcClient>();
        wrapper->impl = std::make_unique<GrpcClientImpl>(std::string(endpoint));
        return wrapper.release();
    } catch (...) {
        return nullptr;
    }
}

void postfga_client_fini(GrpcClient *client)
{
    if (!client)
        return;

    // impl 소멸자에서 CQ/스레드 정리
    delete client;
}

bool postfga_client_check_sync(GrpcClient *client,
                              const GrpcCheckRequest *request,
                              CheckResponse *response)
{
    if (!client || !client->impl || !request || !response)
        return false;

    return client->impl->CheckSync(*request, *response);
}

bool postfga_client_check_async(GrpcClient *client,
                             const GrpcCheckRequest *request,
                             CheckCallback callback,
                             void *user_data)
{
    if (!client || !client->impl || !request || !callback)
        return false;

    return client->impl->CheckAsync(*request, callback, user_data);
}

int postfga_client_poll(GrpcClient *client, int timeout_ms)
{
    (void) client;
    (void) timeout_ms;
    // CQ 전용 스레드를 이미 돌리고 있기 때문에 현재는 할 일 없음.
    return 0;
}

bool postfga_client_is_healthy(GrpcClient *client)
{
    if (!client || !client->impl)
        return false;

    return client->impl->IsHealthy();
}

bool postfga_client_write_sync(GrpcClient *client,
                              const GrpcWriteRequest *request,
                              WriteResponse *response)
{
    if (!client || !client->impl || !request || !response)
        return false;

    // TODO: Implement WriteSync in GrpcClientImpl
    // For now, return a stub implementation
    response->success = true;
    response->error_code = 0;
    response->error_message[0] = '\0';
    return true;
}

bool postfga_client_write_async(GrpcClient *client,
                               const GrpcWriteRequest *request,
                               WriteCallback callback,
                               void *user_data)
{
    if (!client || !client->impl || !request || !callback)
        return false;

    // TODO: Implement WriteAsync in GrpcClientImpl
    // For now, immediately call callback with success
    WriteResponse response;
    response.success = true;
    response.error_code = 0;
    response.error_message[0] = '\0';
    callback(&response, user_data);
    return true;
}

} // extern "C"
