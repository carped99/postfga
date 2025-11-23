#pragma once
#include <string>

namespace postfga
{
    struct GrpcTlsOptions
    {
        bool use_tls = false;
        std::string root_certs;
        std::string client_cert;
        std::string client_key;
        bool insecure_skip_verify = false;
        std::string server_name_override;
    };

    struct GrpcChannelOptions
    {
        int max_message_size = 4 * 1024 * 1024; // -1 = 제한 없음

        int keepalive_time_ms = 0;
        int keepalive_timeout_ms = 0;
        bool keepalive_without_calls = false;

        int idle_timeout_ms = 0;

        std::string load_balancing_policy; // 예: "round_robin"
    };
} // namespace postfga::config