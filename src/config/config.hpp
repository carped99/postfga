#pragma once

#include <cstdint>
#include <string>
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
        std::string model_id; // optional, can be empty

        std::chrono::milliseconds timeout = std::chrono::milliseconds(10000); // 기본 10초

        GrpcTlsOptions tls;
        GrpcChannelOptions channel;
        RetryOptions retry;
        ConcurrencyOptions concurrency;
    };

} // namespace postfga
