#pragma once

#include <semaphore>
#include <limits>

namespace postfga::util {

/**
 * @brief C++20 std::counting_semaphore 기반 동시 실행 제한기.
 *
 * - max_value > 0  : 동시에 실행 가능한 최대 개수 제한
 * - max_value <= 0 : 제한 없음 (무제한 모드, 세마포어 사용 안 함)
 *
 * acquire()는 non-blocking이며, 성공 시 true인 Guard를, 실패 시 false인 Guard를 반환한다.
 * acquire_blocking()은 제한이 있을 때는 블로킹, 제한 없을 때는 그냥 통과한다.
 */
class Counter
{
public:
    class Guard;

    /**
     * @param max_value
     *   - > 0 : 허용 가능한 최대 동시 실행 개수
     *   - <=0 : 제한 없음
     */
    explicit Counter(int max_value);

    Counter(const Counter&) = delete;
    Counter& operator=(const Counter&) = delete;

    /**
     * @brief blocking 토큰 획득.
     *
     * - 제한 모드(max_value > 0): 세마포어 acquire로 블로킹 후 true Guard 반환
     * - 무제한 모드(max_value <= 0): 세마포어를 사용하지 않고 false Guard 반환
     */
    Guard acquire();

    /**
     * @brief non-blocking 토큰 획득 시도.
     *
     * - 제한 모드(max_value > 0):
     *     - 세마포어 try_acquire 성공 시, true 상태의 Guard 반환
     *     - 실패 시 false 상태의 Guard 반환
     * - 무제한 모드(max_value <= 0):
     *     - 세마포어를 사용하지 않으며, false 상태의 Guard 반환
     *       (실제 제한을 하지 않으므로 Guard는 단순히 스코프 관리용)
     */
    Guard try_acquire();

    /**
     * @brief 설정된 최대 동시 실행 개수 반환.
     */
    int max() const noexcept { return max_value_; }

private:
    using semaphore_t = std::counting_semaphore<std::numeric_limits<int>::max()>;    

    semaphore_t semaphore_;
    const int   max_value_;
};

/**
 * @brief CounterLimiter의 RAII 토큰.
 *
 * - truthy(==true) : 세마포어 토큰을 하나 보유한 상태
 * - falsy(==false) : 토큰을 보유하지 않은 상태
 *
 * move-only 이며, 소멸 시 토큰을 자동으로 반환(release)한다.
 */
class Counter::Guard
{
public:
    Guard() noexcept;
    Guard(Counter::semaphore_t* sem, bool acquired) noexcept;

    Guard(Guard&& other) noexcept;
    Guard& operator=(Guard&& other) noexcept;

    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    ~Guard();

    explicit operator bool() const noexcept { return acquired_; }

private:
    void release() noexcept;

    Counter::semaphore_t* sem_;
    bool                            acquired_;
};

} // namespace postfga::util
