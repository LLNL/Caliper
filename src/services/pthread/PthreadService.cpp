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

///@file  PthreadService.cpp
///@brief Service for pthreads-based threading runtimes

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>

#include <cstdlib>
#include <pthread.h>


using namespace cali;
using namespace std;

namespace
{

pthread_key_t thread_env_key;

void release_scope(void* ctx)
{
    Caliper c = Caliper::instance();

    if (c)
        c.release_scope(static_cast<Caliper::Scope*>(ctx));
}

void save_scope(Caliper::Scope* s)
{
    pthread_setspecific(thread_env_key, s);
}

Caliper::Scope*
get_thread_scope(Caliper* c)
{
    Caliper::Scope* ctxbuf = static_cast<Caliper::Scope*>(pthread_getspecific(thread_env_key));

    if (!ctxbuf) {
        ctxbuf = c->create_scope(CALI_SCOPE_THREAD);
        save_scope(ctxbuf);
    }

    return ctxbuf;
}

/// Initialization routine. 
/// Create the thread-local storage key, register the environment callback, and 
/// map current (initialization) thread to default environment
void pthreadservice_initialize(Caliper* c)
{
    pthread_key_create(&thread_env_key, &release_scope);
    save_scope(c->default_scope(CALI_SCOPE_THREAD));

    c->set_scope_callback(CALI_SCOPE_THREAD, &get_thread_scope);

    Log(1).stream() << "Registered pthread service" << endl;
}

} // namespace [anonymous]

namespace cali
{
    CaliperService PthreadService { "pthread", ::pthreadservice_initialize };
}
