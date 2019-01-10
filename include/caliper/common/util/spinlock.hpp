/// \file  spinlock.hpp
/// \brief spinlock class

#ifndef UTIL_SPINLOCK_HPP
#define UTIL_SPINLOCK_HPP

#include <atomic>

namespace util
{
    
class spinlock {
    std::atomic_flag m_lock;

public:

    constexpr spinlock()
        : m_lock { ATOMIC_FLAG_INIT }
        { }

    void lock() {
        while (m_lock.test_and_set(std::memory_order_acquire))
            ;
    }

    void unlock() {
        m_lock.clear(std::memory_order_release);
    }
};

} // namespace

#endif
