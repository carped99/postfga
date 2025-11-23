#pragma once

#include <string>
#include <cstdint>
#include <thread>

#include "grpc.hpp"
#include "retry.hpp"

namespace postfga
{

    struct ConcurrencyOptions
    {
        int max_concurrency = 0; // 0 = 제한 없음
        std::size_t worker_threads = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    };

    struct Config
    {
        std::string endpoint;
        std::string store_id;
        std::string authorization_model_id; // optional, can be empty

        std::uint32_t timeout_ms = 5000;
        
        GrpcTlsOptions       tls;
        GrpcChannelOptions   channel;
        RetryOptions         retry;
        ConcurrencyOptions   concurrency;
    };

} // namespace postfga