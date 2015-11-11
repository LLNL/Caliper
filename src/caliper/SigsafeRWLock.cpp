// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
