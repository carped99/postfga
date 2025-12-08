#include "counter.hpp"

namespace fga::util {

// ------------------ Counter ------------------

Counter::Counter(int max_value)
    : semaphore_(max_value > 0 ? max_value : std::numeric_limits<int>::max())
    , max_value_(max_value)
{
}


// blocking acquire
Counter::Guard Counter::acquire()
{
    // 무제한 모드: 세마포어를 사용하지 않고 바로 통과
    if (max_value_ <= 0)
        return Guard(nullptr, true);

    semaphore_.acquire();
    return Guard(&semaphore_, true);
}

// non-blocking acquire
Counter::Guard Counter::try_acquire()
{
    if (max_value_ <= 0)
        return Guard(nullptr, true);

    if (semaphore_.try_acquire())
        return Guard(&semaphore_, true);

    return Guard(nullptr, false);
}

// ------------------ Guard ------------------

Counter::Guard::Guard() noexcept
    : sem_(nullptr)
    , acquired_(false)
{
}

Counter::Guard::Guard(Counter::semaphore_t* sem, bool acquired) noexcept
    : sem_(sem)
    , acquired_(acquired)
{
}

Counter::Guard::Guard(Guard&& other) noexcept
    : sem_(other.sem_)
    , acquired_(other.acquired_)
{
    other.sem_      = nullptr;
    other.acquired_ = false;
}

Counter::Guard& Counter::Guard::operator=(Guard&& other) noexcept
{
    if (this != &other)
    {
        // 기존에 들고 있던 토큰 반환
        release();

        sem_      = other.sem_;
        acquired_ = other.acquired_;

        other.sem_      = nullptr;
        other.acquired_ = false;
    }
    return *this;
}

Counter::Guard::~Guard()
{
    release();
}

void Counter::Guard::release() noexcept
{
    if (sem_ && acquired_)
    {
        sem_->release();
    }
    sem_      = nullptr;
    acquired_ = false;
}

} // namespace fga::util
