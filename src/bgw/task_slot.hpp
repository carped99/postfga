// grpc_async_slot.hpp
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>

// 앞에서 C 쪽에 정의된 shared memory slot
extern "C"
{
    typedef struct FgaChannelSlot FgaChannelSlot;
}

struct GrpcAsyncSlot
{
    uint16_t slot_index = 0; // shared memory slot index
    FgaChannelSlot *slot_ptr = nullptr;

    // 필요하면 여기에 gRPC 핸들, retry 상태 등을 추가
    // grpc_call* call = nullptr;
    // ...
};

class GrpcAsyncSlotPool
{
public:
    explicit GrpcAsyncSlotPool(std::size_t capacity)
        : capacity_(capacity)
    {
        slots_.reserve(capacity_);
        free_list_.reserve(capacity_);

        for (std::size_t i = 0; i < capacity_; ++i)
        {
            slots_.emplace_back(std::make_unique<GrpcAsyncSlot>());
            free_list_.push_back(slots_[i].get());
        }
    }

    // 최대 상한을 넘으면 여기서 "대기"한다.
    GrpcAsyncSlot *acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]
                 { return !free_list_.empty(); });

        GrpcAsyncSlot *s = free_list_.back();
        free_list_.pop_back();
        ++in_use_;

        // 사용 전에 깔끔하게 리셋
        *s = GrpcAsyncSlot{};
        return s;
    }

    // non-blocking 버전이 필요하면 이런 식으로도 가능
    GrpcAsyncSlot *try_acquire()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_list_.empty())
            return nullptr;

        GrpcAsyncSlot *s = free_list_.back();
        free_list_.pop_back();
        ++in_use_;

        *s = GrpcAsyncSlot{};
        return s;
    }

    void release(GrpcAsyncSlot *slot)
    {
        if (!slot)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        free_list_.push_back(slot);
        --in_use_;
        cv_.notify_one();
    }

    std::size_t capacity() const noexcept { return capacity_; }

    std::size_t in_use() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return in_use_;
    }

private:
    const std::size_t capacity_;
    std::vector<std::unique_ptr<GrpcAsyncSlot>> slots_;
    std::vector<GrpcAsyncSlot *> free_list_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t in_use_ = 0;
};
