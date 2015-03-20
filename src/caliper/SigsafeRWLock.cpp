/// @file SigsafeRWLock.cpp
/// SigsafeRWLock implementation

#include "SigsafeRWLock.h"

using namespace cali;

volatile sig_atomic_t SigsafeRWLock::s_global_signal_flag { 0 };

SigsafeRWLock::SigsafeRWLock()
    : m_sigwlock { 0 }, m_sigrlock { 0 }, m_sig_wflag { 0 }, m_sig_rflag { 0 }
{
    pthread_rwlock_init(&m_rwlock, NULL);
}

SigsafeRWLock::~SigsafeRWLock()
{
    pthread_rwlock_destroy(&m_rwlock);
}

void SigsafeRWLock::rlock()
{
    pthread_rwlock_rdlock(&m_rwlock);
    ++m_sigrlock;

    // Spin while signal handler wlocked this mutex
    while ( m_sig_wflag != 0 )
        ;
}

void SigsafeRWLock::wlock()
{
    pthread_rwlock_wrlock(&m_rwlock);
    ++m_sigwlock;

    // Spin while signal handler rlocked or wlocked this mutex
    while ( m_sig_rflag != 0 )
        ;
    while ( m_sig_wflag != 0 )
        ;
}

bool SigsafeRWLock::sig_try_rlock()
{
    if (m_sigwlock != 0)
        return false;

    m_sig_rflag = 1;

    return true;
}

bool SigsafeRWLock::sig_try_wlock()
{
    if (m_sigwlock != 0 || m_sigrlock != 0)
        return false;

    m_sig_wflag = 1;

    return true;
}

void SigsafeRWLock::unlock()
{
    if (m_sigwlock)
        --m_sigrlock;
    if (m_sigrlock)
        --m_sigwlock;

    pthread_rwlock_unlock(&m_rwlock);
}

void SigsafeRWLock::sig_unlock()
{
    m_sig_wflag = 0;
    m_sig_rflag = 0;
}
