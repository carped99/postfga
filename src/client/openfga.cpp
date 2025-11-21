// openfga.cpp
#include "openfga.hpp"

#include <algorithm>
#include <cstring>
#include <thread>

namespace postfga::client {

namespace {

constexpr int kMaxGrpcMessageSize = 4 * 1024 * 1024;

// "type:id" 형태로 user/object 문자열 만들기
inline std::string make_user(const GrpcCheckRequest& r)
{
    std::string user;
    user.reserve(64);
    user.append(r.subject_type).append(":").append(r.subject_id);
    return user;
}

inline std::string make_object(const GrpcCheckRequest& r)
{
    std::string obj;
    obj.reserve(64);
    obj.append(r.object_type).append(":").append(r.object_id);
    return obj;
}

} // namespace

/* ========================================================================
 * ctor / dtor
 * ====================================================================== */

OpenFgaGrpcClient::OpenFgaGrpcClient(const Config& config)
    : config_(config)
    , pool_(config.worker_threads)
{
    grpc::ChannelArguments args;
    args.SetMaxReceiveMessageSize(kMaxGrpcMessageSize);
    args.SetMaxSendMessageSize(kMaxGrpcMessageSize);

    channel_ = grpc::CreateCustomChannel(
        config_.endpoint,
        grpc::InsecureChannelCredentials(),  // TLS 쓰면 수정
        args
    );

    stub_ = openfga::v1::OpenFgaService::NewStub(channel_);
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
    auto deadline = std::chrono::system_clock::now() +
                    std::chrono::milliseconds(config_.timeout_ms);
    return channel_->WaitForConnected(deadline);
}

void OpenFgaGrpcClient::async_check(const GrpcCheckRequest& req, CheckHandler handler)
{
    if (!handler || stopping_.load(std::memory_order_relaxed))
        return;

    // concurrency 제한
    if (config_.max_concurrency > 0 &&
        inflight_.load(std::memory_order_relaxed) >= config_.max_concurrency)
    {
        CheckResponse resp{};
        resp.grpc_status_code   = static_cast<int>(grpc::StatusCode::RESOURCE_EXHAUSTED);
        resp.openfga_error_code = 0;
        resp.allowed            = 0;
        std::strncpy(resp.error_message,
                     "OpenFGA client: max_concurrency exceeded",
                     sizeof(resp.error_message) - 1);
        handler(resp);
        return;
    }

    inflight_.fetch_add(1, std::memory_order_relaxed);

    auto self = shared_from_this();
    asio::post(pool_, [self, req, handler]() mutable {
        CheckResponse resp = self->do_check_with_retry(req);
        self->inflight_.fetch_sub(1, std::memory_order_relaxed);

        // handler는 worker thread에서 바로 호출 (원하면 여기서 asio::post로 다른 executor에 넘길 수 있음)
        handler(resp);
    });
}

void OpenFgaGrpcClient::async_write(const GrpcWriteRequest& req, WriteHandler handler)
{
    if (!handler || stopping_.load(std::memory_order_relaxed))
        return;

    if (config_.max_concurrency > 0 &&
        inflight_.load(std::memory_order_relaxed) >= config_.max_concurrency)
    {
        WriteResponse resp{};
        resp.grpc_status_code   = static_cast<int>(grpc::StatusCode::RESOURCE_EXHAUSTED);
        resp.openfga_error_code = 0;
        resp.success            = 0;
        std::strncpy(resp.error_message,
                     "OpenFGA client: max_concurrency exceeded",
                     sizeof(resp.error_message) - 1);
        handler(resp);
        return;
    }

    inflight_.fetch_add(1, std::memory_order_relaxed);

    auto self = shared_from_this();
    asio::post(pool_, [self, req, handler]() mutable {
        WriteResponse resp = self->do_write_with_retry(req);
        self->inflight_.fetch_sub(1, std::memory_order_relaxed);
        handler(resp);
    });
}

void OpenFgaGrpcClient::shutdown()
{
    bool expected = false;
    if (!stopping_.compare_exchange_strong(expected, true))
        return; // 이미 멈추는 중

    // 더 이상 새 작업은 들어오지 않음
    pool_.join();
}

/* ========================================================================
 * 내부: retry 래핑
 * ====================================================================== */

CheckResponse OpenFgaGrpcClient::do_check_with_retry(const GrpcCheckRequest& req) const
{
    CheckResponse resp{};
    for (int attempt = 0;; ++attempt)
    {
        resp = do_check_once(req);
        if (resp.grpc_status_code == 0 /* OK*/ ||
            !should_retry(grpc::Status(static_cast<grpc::StatusCode>(resp.grpc_status_code), ""),
                          attempt))
        {
            break;
        }

        int backoff = next_backoff_ms(attempt);
        if (backoff > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
    }
    return resp;
}

WriteResponse OpenFgaGrpcClient::do_write_with_retry(const GrpcWriteRequest& req) const
{
    WriteResponse resp{};
    for (int attempt = 0;; ++attempt)
    {
        resp = do_write_once(req);
        if (resp.grpc_status_code == 0 /* OK*/ ||
            !should_retry(grpc::Status(static_cast<grpc::StatusCode>(resp.grpc_status_code), ""),
                          attempt))
        {
            break;
        }

        int backoff = next_backoff_ms(attempt);
        if (backoff > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
    }
    return resp;
}

/* ========================================================================
 * 내부: 단일 호출
 * ====================================================================== */

CheckResponse OpenFgaGrpcClient::do_check_once(const GrpcCheckRequest& req) const
{
    CheckResponse out{};
    out.grpc_status_code   = 0;
    out.openfga_error_code = 0;
    out.allowed            = 0;
    out.error_message[0]   = '\0';

    grpc::ClientContext ctx;
    openfga::v1::CheckRequest  greq;
    openfga::v1::CheckResponse gresp;

    fill_check_request(req, greq);

    auto deadline = std::chrono::system_clock::now() +
                    std::chrono::milliseconds(config_.timeout_ms);
    ctx.set_deadline(deadline);

    grpc::Status status = stub_->Check(&ctx, greq, &gresp);
    if (!status.ok())
    {
        out.grpc_status_code = static_cast<int>(status.error_code());
        std::strncpy(out.error_message,
                     status.error_message().c_str(),
                     sizeof(out.error_message) - 1);
        return out;
    }

    // 실제 proto 필드 확인 필요 (일반적으로 allowed() bool)
    out.allowed          = gresp.allowed() ? 1 : 0;
    out.grpc_status_code = 0;
    return out;
}

WriteResponse OpenFgaGrpcClient::do_write_once(const GrpcWriteRequest& req) const
{
    WriteResponse out{};
    out.grpc_status_code   = 0;
    out.openfga_error_code = 0;
    out.success            = 0;
    out.error_message[0]   = '\0';

    grpc::ClientContext ctx;
    openfga::v1::WriteRequest  greq;
    openfga::v1::WriteResponse gresp;

    fill_write_request(req, greq);

    auto deadline = std::chrono::system_clock::now() +
                    std::chrono::milliseconds(config_.timeout_ms);
    ctx.set_deadline(deadline);

    grpc::Status status = stub_->Write(&ctx, greq, &gresp);
    if (!status.ok())
    {
        out.grpc_status_code = static_cast<int>(status.error_code());
        std::strncpy(out.error_message,
                     status.error_message().c_str(),
                     sizeof(out.error_message) - 1);
        return out;
    }

    out.success          = 1;
    out.grpc_status_code = 0;
    return out;
}

/* ========================================================================
 * retry/backoff
 * ====================================================================== */

bool OpenFgaGrpcClient::should_retry(const grpc::Status& status, int attempt) const
{
    if (status.ok())
        return false;

    if (attempt >= config_.max_retries)
        return false;

    using grpc::StatusCode;
    switch (status.error_code())
    {
        case StatusCode::UNAVAILABLE:
        case StatusCode::DEADLINE_EXCEEDED:
        case StatusCode::ABORTED:
        case StatusCode::INTERNAL:
            return true;
        default:
            return false;
    }
}

int OpenFgaGrpcClient::next_backoff_ms(int attempt) const
{
    if (attempt <= 0)
        return 0;

    int backoff = config_.initial_backoff_ms * (1 << (attempt - 1));
    backoff = std::min(backoff, config_.max_backoff_ms);
    return backoff;
}

/* ========================================================================
 * proto 매핑
 * ====================================================================== */

void OpenFgaGrpcClient::fill_check_request(const GrpcCheckRequest& in,
                                           openfga::v1::CheckRequest& out) const
{
    auto* tuple_key = out.mutable_tuple_key();
    tuple_key->set_user(make_user(in));
    tuple_key->set_relation(in.relation);
    tuple_key->set_object(make_object(in));

    if (in.store_id && in.store_id[0] != '\0')
        out.set_store_id(in.store_id);
    else if (!config_.default_store_id.empty())
        out.set_store_id(config_.default_store_id);

    if (in.authorization_model_id && in.authorization_model_id[0] != '\0')
        out.set_authorization_model_id(in.authorization_model_id);
    else if (!config_.default_auth_model_id.empty())
        out.set_authorization_model_id(config_.default_auth_model_id);
}

void OpenFgaGrpcClient::fill_write_request(const GrpcWriteRequest& in,
                                           openfga::v1::WriteRequest& out) const
{
    if (in.store_id && in.store_id[0] != '\0')
        out.set_store_id(in.store_id);
    else if (!config_.default_store_id.empty())
        out.set_store_id(config_.default_store_id);

    if (in.authorization_model_id && in.authorization_model_id[0] != '\0')
        out.set_authorization_model_id(in.authorization_model_id);
    else if (!config_.default_auth_model_id.empty())
        out.set_authorization_model_id(config_.default_auth_model_id);

    // TODO: in.tuples_json 을 실제 proto 구조로 파싱해서 매핑
    //   out.mutable_writes()->mutable_tuple_keys()->Add(...);
    //   out.mutable_deletes()->mutable_tuple_keys()->Add(...);
}

} // namespace postfga::client
