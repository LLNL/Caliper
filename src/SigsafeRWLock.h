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
    pthread_rwlock_t      m_rwlock;
    volatile sig_atomic_t m_sigwlock;
    volatile sig_atomic_t m_sigrlock;

public:

    SigsafeRWLock();

    ~SigsafeRWLock();

    void rlock();
    void wlock();

    bool sig_try_read();
    bool sig_try_write();

    void unlock();
};

}

#endif
