#pragma once

#include <string>
#include <cstdint>
#include <thread>

namespace postfga::client {

struct Config {
    std::string endpoint;
    std::string store_id;
    std::string authorization_model_id;  // optional, can be empty
    std::uint32_t timeout_ms = 5000;
    bool use_tls = false;

    int max_retries        = 2;         // 재시도 횟수 (0이면 재시도 없음)
    int initial_backoff_ms = 50;        // 첫 backoff
    int max_backoff_ms     = 500;       // 최대 backoff
    int max_concurrency    = 0;         // 0 = 제한 없음

    std::size_t worker_threads = std::max<std::size_t>(1, std::thread::hardware_concurrency());
};

} // namespace postfga::client
