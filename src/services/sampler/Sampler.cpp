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

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>

#include <unistd.h>

#include <sys/syscall.h>
#include <sys/types.h>
#include <ucontext.h>

using namespace cali;
using namespace std;

namespace 
{
    Attribute timer_attr   { Attribute::invalid };
    Attribute sampler_attr { Attribute::invalid };

    cali_id_t sampler_attr_id    = CALI_INV_ID;
    
    ConfigSet config;

    int       nsec_interval       = 0;
    int       sample_contexts     = 0;

    int       n_samples           = 0;
    int       n_processed_samples = 0;

    static const ConfigSet::Entry s_configdata[] = {
        { "frequency", CALI_TYPE_INT, "10",
          "Sampling frequency (in Hz)",
          "Sampling frequency (in Hz)"
        },
        { "add_shared_context", CALI_TYPE_BOOL, "true",
          "Capture process-wide context information",
          "Capture process-wide context information in addition to thread-local context"
        },
        ConfigSet::Terminator
    };

    void on_prof(int sig, siginfo_t *info, void *context)
    {
        ++n_samples;
        
        Caliper c = Caliper::sigsafe_instance();

        if (!c)
            return;
        
        ucontext_t *ucontext = (ucontext_t *) context;

        uint64_t  pc = static_cast<uint64_t>(ucontext->uc_mcontext.gregs[REG_RIP]);
        Variant v_pc(CALI_TYPE_ADDR, &pc, sizeof(uint64_t));

        SnapshotRecord trigger_info(1, &sampler_attr_id, &v_pc);

        c.push_snapshot(sample_contexts, &trigger_info);

        ++n_processed_samples;
    }

    void setup_signal()
    {
        sigset_t sigset;

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGPROF);
        sigprocmask(SIG_UNBLOCK, &sigset, NULL);

        struct sigaction act;
        
        memset(&act, 0, sizeof(act));

        act.sa_sigaction = on_prof;
        act.sa_flags     = SA_RESTART | SA_SIGINFO;

        sigaction(SIGPROF, &act, NULL);
    }

    void clear_signal()
    {
        signal(SIGPROF, SIG_IGN);
    }

    void setup_settimer(Caliper* c)
    {
        struct sigevent sev;
   
        std::memset(&sev, 0, sizeof(sev));
        
        sev.sigev_notify   = SIGEV_THREAD_ID;     // Linux-specific!
        sev._sigev_un._tid = syscall(SYS_gettid);
        sev.sigev_signo    = SIGPROF;

        timer_t timer;
   
        if (timer_create(CLOCK_MONOTONIC, &sev, &timer) == -1) {
            Log(0).stream() << "sampler: timer_create() failed" << std::endl;
            return;
        }
        
        struct itimerspec spec;

        spec.it_interval.tv_sec  = 0;
        spec.it_interval.tv_nsec = nsec_interval;
        spec.it_value.tv_sec     = 0;
        spec.it_value.tv_nsec    = nsec_interval;

        if (timer_settime(timer, 0, &spec, NULL) == -1) {
            Log(0).stream() << "sampler: timer_settime() failed" << std::endl;
            return;
        }

        // FIXME: We make the assumption that timer_t is a 8-byte value
        Variant v_timer(CALI_TYPE_ADDR, &timer, sizeof(void*));
        
        c->set(timer_attr, v_timer);

        Log(2).stream() << "Registered timer " << v_timer << endl;
    }

    void clear_timer(Caliper* c) {
        Entry e = c->get(timer_attr);

        if (e.is_empty()) {
            Log(2).stream() << "Timer attribute not found " << endl;
            return;
        }
        
        Variant v_timer = e.value();

        if (v_timer.empty())
            return;

        timer_t timer;
        memcpy(&timer, v_timer.data(), sizeof(void*));

        Log(2).stream() << "Deleting timer " << v_timer << endl;

        timer_delete(timer);
    }
    
    void create_scope_cb(Caliper* c, cali_context_scope_t scope) {
        if (scope == CALI_SCOPE_THREAD)
            setup_settimer(c);
    }

    void release_scope_cb(Caliper* c, cali_context_scope_t scope) {
        if (scope == CALI_SCOPE_THREAD)
            clear_timer(c);
    }

    void finish_cb(Caliper* c) {
        clear_timer(c);
        clear_signal();

        Log(1).stream() << "Sampler: processed " << n_processed_samples << " samples ("
                        << n_samples << " total, "
                        << n_samples - n_processed_samples << " dropped)." << endl;
    }
    
    void sampler_register(Caliper* c)
    {
        config = RuntimeConfig::init("sampler", s_configdata);


        Attribute symbol_class_attr = c->get_attribute("class.symboladdress");
        Variant v_true(true);

        timer_attr =
            c->create_attribute("cali.sampler.timer", CALI_TYPE_ADDR,
                                CALI_ATTR_SCOPE_THREAD |
                                CALI_ATTR_SKIP_EVENTS  |
                                CALI_ATTR_ASVALUE      |
                                CALI_ATTR_HIDDEN);
        sampler_attr =
            c->create_attribute("cali.sampler.pc", CALI_TYPE_ADDR,
                                CALI_ATTR_SCOPE_THREAD |
                                CALI_ATTR_SKIP_EVENTS  |
                                CALI_ATTR_ASVALUE,
                                1, &symbol_class_attr, &v_true);

        sampler_attr_id = sampler_attr.id();

        int frequency = config.get("frequency").to_int();
        
        // some sanity checking
        frequency     = std::min(std::max(frequency, 1), 10000);
        nsec_interval = 1000000000 / frequency;

        sample_contexts = CALI_SCOPE_THREAD;

        if (config.get("add_shared_context").to_bool() == true)
            sample_contexts |= CALI_SCOPE_PROCESS;
        
        c->events().create_scope_evt.connect(create_scope_cb);
        c->events().release_scope_evt.connect(release_scope_cb);
        c->events().finish_evt.connect(finish_cb);

        setup_signal();
        setup_settimer(c);
        
        Log(1).stream() << "Registered sampler service. Using "
                        << frequency << "Hz sampling frequency." << endl;
    }

} // namespace

namespace cali
{
    CaliperService sampler_service { "sampler", &::sampler_register };
}
