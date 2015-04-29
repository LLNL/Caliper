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
    static pthread_key_t s_sig_key;
    pthread_rwlock_t     m_rwlock;

public:

    SigsafeRWLock();

    ~SigsafeRWLock();

    static void init();
    static bool is_thread_locked();

    void rlock();
    void wlock();

    void unlock();
};

}

#endif
