/// @file SigsafeRWLock.cpp
/// SigsafeRWLock implementation

#include "SigsafeRWLock.h"

#include <iostream>

pthread_key_t cali::SigsafeRWLock::s_sig_key;

using namespace cali;

SigsafeRWLock::SigsafeRWLock()
{
    pthread_rwlock_init(&m_rwlock, NULL);
}

SigsafeRWLock::~SigsafeRWLock()
{
    pthread_rwlock_destroy(&m_rwlock);
}

void SigsafeRWLock::init()
{
    pthread_key_create(&s_sig_key, NULL);
}

bool SigsafeRWLock::is_thread_locked()
{
    volatile sig_atomic_t *flagptr = static_cast<volatile sig_atomic_t*>(pthread_getspecific(s_sig_key));

    return (flagptr == nullptr) || (*flagptr) > 0;
}

void SigsafeRWLock::rlock()
{
    sig_atomic_t *flagptr = static_cast<sig_atomic_t*>(pthread_getspecific(s_sig_key));

    if (flagptr == nullptr) {
        flagptr  = new sig_atomic_t;
        *flagptr = 0;
        pthread_setspecific(s_sig_key, flagptr);
    }

    ++(*flagptr);

    pthread_rwlock_rdlock(&m_rwlock);
}

void SigsafeRWLock::wlock()
{
    sig_atomic_t *flagptr = static_cast<sig_atomic_t*>(pthread_getspecific(s_sig_key));

    if (!flagptr) {
        flagptr  = new sig_atomic_t;
        *flagptr = 0;
        pthread_setspecific(s_sig_key, flagptr);
    }

    ++(*flagptr);

    pthread_rwlock_wrlock(&m_rwlock);
}

void SigsafeRWLock::unlock()
{
    pthread_rwlock_unlock(&m_rwlock);

    volatile sig_atomic_t *flagptr = static_cast<volatile sig_atomic_t*>(pthread_getspecific(s_sig_key));
    --(*flagptr);
}
