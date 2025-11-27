#pragma once

namespace postfga
{
    struct RetryOptions
    {
        int max_retries = 2;
        int initial_backoff_ms = 50;
        int max_backoff_ms = 500;
        float backoff_multiplier = 2.0f;

        bool retry_unavailable = true;
        bool retry_deadline_exceeded = false;
    };
} // namespace postfga
