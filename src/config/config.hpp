#pragma once

#include <cstdint>
#include <string>
#include <thread>

namespace fga
{
    struct RetryOptions
    {
        int max_retries = 2;
        int initial_backoff_ms = 50;
        int max_backoff_ms = 500;
        float backoff_multiplier = 2.0f;

        bool retry_unavailable = true;
        bool retry_deadline_exceeded = false;

        bool operator==(const RetryOptions&) const = default;
    };

    struct GrpcTlsOptions
    {
        bool use_tls = false;
        std::string root_certs;
        std::string client_cert;
        std::string client_key;
        bool insecure_skip_verify = false;
        std::string server_name_override;

        bool operator==(const GrpcTlsOptions&) const = default;
    };

    struct GrpcChannelOptions
    {
        int max_message_size = 4 * 1024 * 1024; // -1 = 제한 없음

        int keepalive_time_ms = 0;
        int keepalive_timeout_ms = 0;
        bool keepalive_without_calls = false;

        int idle_timeout_ms = 0;

        std::string load_balancing_policy; // 예: "round_robin"

        bool operator==(const GrpcChannelOptions&) const = default;
    };

    struct ConcurrencyOptions
    {
        int max_concurrency = 0; // 0 = 제한 없음
        std::size_t worker_threads = std::max<std::size_t>(1, std::thread::hardware_concurrency());

        bool operator==(const ConcurrencyOptions&) const = default;
    };

    struct Config
    {
        std::string endpoint;
        std::chrono::milliseconds timeout = std::chrono::milliseconds(10000); // 기본 10초

        GrpcTlsOptions tls;
        GrpcChannelOptions channel;
        RetryOptions retry;
        ConcurrencyOptions concurrency;

        bool operator==(const Config&) const = default;
    };

    Config load_config_from_guc();

} // namespace fga
