// grpc_async.hpp
#pragma once

#include <atomic>
#include <cstdint>
#include <array>
#include <vector>
#include <mutex>
#include <optional>

namespace postfga::bgw
{

    enum class SlotState : std::uint8_t
    {
        Idle = 0,
        Inflight,
        Completed,
    };

    struct AsyncSlot
    {
        std::atomic<SlotState> state{SlotState::Idle};

        std::uint32_t token{}; // PG FgaCheckSlot을 찾기 위한 식별자
        bool allowed{};
        std::array<char, 64> error_msg{};
    };

} // namespace postfga::bgw