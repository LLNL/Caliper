/// @file SigsafeRWLock.cpp
/// SigsafeRWLock implementation

#include "SigsafeRWLock.h"

using namespace cali;


SigsafeRWLock::SigsafeRWLock()
    : m_sigwlock { 0 }, m_sigrlock { 0 }
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
}

void SigsafeRWLock::wlock()
{
    pthread_rwlock_wrlock(&m_rwlock);
    ++m_sigwlock;
}

bool SigsafeRWLock::sig_try_read()
{
    return m_sigwlock == 0;        
}

bool SigsafeRWLock::sig_try_write()
{
    return m_sigwlock == 0 && m_sigrlock == 0;        
}

void SigsafeRWLock::unlock()
{
    if (m_sigwlock)
        --m_sigrlock;
    if (m_sigrlock)
        --m_sigwlock;

    pthread_rwlock_unlock(&m_rwlock);
}
