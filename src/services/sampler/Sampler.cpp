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

/// @file  Sampler.cpp
/// @brief Caliper sampler service

#include "../CaliperService.h"

#include <Caliper.h>
#include <Snapshot.h>

#include <Log.h>

#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>

#include <unistd.h>

#include <errno.h>

#include <sys/types.h>
#include <ucontext.h>



using namespace cali;
using namespace std;


namespace 
{

#define NUM_THREADS 1

static timer_t timers[NUM_THREADS];

static void on_prof(int sig, siginfo_t *info, void *context)
{
//   int id = myid;
   int id=0;
   unsigned long pc;
   ucontext_t *ucontext = (ucontext_t *) context;
   totals[id][cur_sec()]++;

   pc = ucontext->uc_mcontext.gregs[REG_RIP];
   pc_samples[id][cur_pc_sample[id]++] = pc;

   if (nosigs) {
      printf("Signal after stop\n");
   }
}

static void setup_signal()
{
    struct sigaction act;
    sigset_t sigset;
    int sig = SIGPROF;

    sigemptyset(&sigset);
    sigaddset(&sigset, sig);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);

    memset(&act, 0, sizeof(act));
    act.sa_sigaction = on_prof;
    act.saflags = SA_RESTART | SA_SIGINFO;
    sigaction(sig, &act, NULL);

}

static void clear_signal()
{
	signal(SIGPROF, SIG_IGN);
}

static void setup_settimer(int id)
{
   int result;
   struct sigevent sev;
   struct itimerspec spec;
   
   std::memset(&sev, 0, sizeof(sev));
   sev.sigev_notify = SIGEV_THREAD_ID;
   sev._sigev_un._tid = gettid();
   sev.sigev_signo = SIGPROF;
   
   result = timer_create(CLOCK_MONOTONIC, &sev, timers+id);
   if (result == -1) {
      perror("Could not timer_create");
      exit(-1);
   }

   spec.it_interval.tv_sec = 0;
   spec.it_interval.tv_nsec = 10000000;
   spec.it_value.tv_sec = 0;
   spec.it_value.tv_nsec = 10000000;

   result = timer_settime(timers[id], 0, &spec, NULL);
   if (result == -1) {
      perror("Could not settime");
      exit(-1);
   }
}

static void stop_settimer(int id)
{
   timer_delete(timers[id]);
}

void sampler_register(Caliper* c)
{
    Log(1).stream() << "Registered sampler service" << endl;
    return;
}

} // namespace

namespace cali
{
    CaliperService SamplerService { "sampler", &::sampler_register };
}
