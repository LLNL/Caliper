/// @file SigsafeRWLock.h
/// Signal-safe read-write lock definition

#ifndef CALI_SIGSAFERWLOCK_H
#define CALI_SIGSAFERWLOCK_H

#include <pthread.h>
#include <signal.h>

namespace cali
{

class SigsafeRWLock
{
    static volatile sig_atomic_t s_global_signal_flag;

    pthread_rwlock_t      m_rwlock;

    volatile sig_atomic_t m_sigwlock;
    volatile sig_atomic_t m_sigrlock;

    volatile sig_atomic_t m_sig_wflag;
    volatile sig_atomic_t m_sig_rflag;

public:

    SigsafeRWLock();

    ~SigsafeRWLock();

    void rlock();
    void wlock();

    bool sig_try_rlock();
    bool sig_try_wlock();

    void unlock();
    void sig_unlock();

    static void enter_signal() {
        s_global_signal_flag = 1;
    }
    static void leave_signal() {
        s_global_signal_flag = 0;
    }

    static bool is_in_signal() { 
        return (s_global_signal_flag != 0);
    }
};

}

#endif
