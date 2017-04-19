// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by Alfredo Gimenez, gimenez1@llnl.gov
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

///@file  PerfEvent.cpp
///@brief perf_event sampling provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <ContextRecord.h>

#include <util/split.hpp>
#include <string.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

#include "PerfEvent.hpp"
#include "mmap_processor.h"

using namespace cali;
using namespace std;

namespace 
{
    Attribute dummy_attr      { Attribute::invalid } ;

    ConfigSet config;

    std::map<std::string, uint64_t> sample_attribute_map = {
        {"ip", PERF_SAMPLE_IP},
        {"callchain", PERF_SAMPLE_CALLCHAIN},
        {"id", PERF_SAMPLE_ID},
        {"stream_id", PERF_SAMPLE_STREAM_ID},
        {"time", PERF_SAMPLE_TIME},
        {"tid", PERF_SAMPLE_TID},
        {"period", PERF_SAMPLE_PERIOD},
        {"cpu", PERF_SAMPLE_CPU},
        {"addr", PERF_SAMPLE_ADDR},
        {"weight", PERF_SAMPLE_WEIGHT},
        {"transaction", PERF_SAMPLE_TRANSACTION},
        {"data_src", PERF_SAMPLE_DATA_SRC}
    };

    static std::vector<std::string> sample_attributes_strvec;
    static uint64_t sample_attributes = 0;

    static const ConfigSet::Entry s_configdata[] = {
        { "event_list", CALI_TYPE_STRING, "c0" /* c0 == INSTRUCTIONS_RETIRED */,
          "Event List",
          "List of events to sample, separated by ':'" 
        },
        { "sample_attributes", CALI_TYPE_STRING, "ip:time:tid:cpu",
          "Sample attributes",
          "Set of attributes to record for each sample, separated by ':'"
        },
        { "frequency", CALI_TYPE_UINT, "10000",
          "Sampling frequency",
          "Number of samples per second to collect (approximately)."
        },
        ConfigSet::Terminator
    };



    static __thread struct thread_sampler tsmp;
    static __thread unsigned int num_good_samples = 0;
    static __thread unsigned int num_bad_samples = 0;

    static int num_events;
    static unsigned int sampling_frequency;
    static std::vector<std::string> events_strvec;
    static std::vector<uint64_t> events;

    static struct perf_event_attr *perf_event_attrs;

    pid_t gettid(void) {
        return (pid_t)syscall(__NR_gettid);
    }

    int perf_event_open(struct perf_event_attr *attr,
                        pid_t pid, int cpu, int group_fd,
                        unsigned long flags) {
        return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
    }


    void thread_sighandler(int sig, siginfo_t *info, void *extra)
    {
        int i;
        int fd = info->si_fd;

        for(i=0; i<num_events; i++)
        {
            if(tsmp.containers[i].fd == fd)
            {
                //std::cerr << "LA CUCARACHA" << std::endl;
                process_sample_buffer(&tsmp.pes,
                                      tsmp.containers[i].attr.sample_type,
                                      //tsmp.proc_parent->handler_fn,
                                      //tsmp.proc_parent->handler_fn_args,
                                      tsmp.containers[i].mmap_buf,
                                      tsmp.pgmsk);
                if(tsmp.pes.ip == 0)
                    std::cerr << "OUCH" << std::endl;
            }
        }

        ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
    }

    static void setup_perf_event_attrs() {

        // Allocate perf_event attrs and populate accordingly
        perf_event_attrs = new struct perf_event_attr[num_events];
        memset(perf_event_attrs, 0, sizeof(struct perf_event_attr)*num_events);

        for (int i=0; i<num_events; i++) {

            // Set up event encoding
            perf_event_attrs[i].disabled = (i == 0) ? 1 : 0;    // Event group leader starts disabled
            perf_event_attrs[i].type = PERF_TYPE_RAW;
            perf_event_attrs[i].config = events[i];
            //perf_event_attrs[i].config1 = TODO
            //perf_event_attrs[i].precise_ip = TODO
            
            perf_event_attrs[i].config = 0x5101cd;          // MEM_TRANS_RETIRED:LATENCY_THRESHOLD
            perf_event_attrs[i].config1 = 7;
            perf_event_attrs[i].precise_ip = 2;

            perf_event_attrs[i].mmap = 1;
            perf_event_attrs[i].mmap_data = 1;
            perf_event_attrs[i].comm = 1;
            perf_event_attrs[i].exclude_user = 0;
            perf_event_attrs[i].exclude_kernel = 0;
            perf_event_attrs[i].exclude_hv = 0;
            perf_event_attrs[i].exclude_idle = 0;
            perf_event_attrs[i].exclude_host = 0;
            perf_event_attrs[i].exclude_guest = 1;
            perf_event_attrs[i].exclusive = 0;
            perf_event_attrs[i].pinned = 0;
            perf_event_attrs[i].sample_id_all = 0;
            perf_event_attrs[i].wakeup_events = 1;
            perf_event_attrs[i].sample_freq = sampling_frequency;
            perf_event_attrs[i].freq = 1;

            perf_event_attrs[i].sample_type = sample_attributes;
        }
    }

    static int init_thread_perf_events() {
        int i;

        size_t mmap_pages = 1;
        size_t pgsz = sysconf(_SC_PAGESIZE);
        size_t mmap_size = (mmap_pages+1)*pgsz;

        tsmp.pgmsk = mmap_pages*pgsz-1;

        tsmp.containers = new struct perf_event_container[num_events];

        tsmp.containers[0].fd = -1;
        
        for(i=0; i<num_events; i++)
        {
            // Create attr according to sample mode
            tsmp.containers[i].fd = perf_event_open(&perf_event_attrs[i], gettid(), -1, tsmp.containers[0].fd, 0);

            if(tsmp.containers[i].fd == -1)
            {
                perror("perf_event_open");
                return 1;
            }

            // Create mmap buffer for samples
            tsmp.containers[i].mmap_buf = (struct perf_event_mmap_page*)
                mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, tsmp.containers[i].fd, 0);

            if(tsmp.containers[i].mmap_buf == MAP_FAILED)
            {
                perror("mmap");
                return 1;
            }
        }
        return 0;
    }

    static int init_thread_sighandler() {

        int i, ret;
        struct f_owner_ex fown_ex;
        struct sigaction sact;

        // Set up signal handler
        memset(&sact, 0, sizeof(sact));
        sact.sa_sigaction = &thread_sighandler;
        sact.sa_flags = SA_SIGINFO;

        ret = sigaction(SIGIO, &sact, NULL);
        if(ret)
        {
            perror("sigaction");
            return ret;
        }

        // Unblock SIGIO signal if necessary
        sigset_t sold, snew;
        sigemptyset(&sold);
        sigemptyset(&snew);
        sigaddset(&snew, SIGIO);

        ret = sigprocmask(SIG_SETMASK, NULL, &sold);
        if(ret)
        {
            perror("sigaction");
            return 1;
        }

        if(sigismember(&sold, SIGIO))
        {
            ret = sigprocmask(SIG_UNBLOCK, &snew, NULL);
            if(ret)
            {
                perror("sigaction");
                return 1;
            }
        } 

        for(i=0; i<num_events; i++)
        {
            // Set perf event tsmp.containers[i].fd to signal
            ret = fcntl(tsmp.containers[i].fd, F_SETSIG, SIGIO);
            if(ret)
            {
                perror("fcntl");
                return 1;
            }
            ret = fcntl(tsmp.containers[i].fd, F_SETFL, O_NONBLOCK | O_ASYNC);
            if(ret)
            {
                perror("fcntl");
                return 1;
            }
            // Set owner to current thread
            fown_ex.type = F_OWNER_TID;
            fown_ex.pid = gettid();
            ret = fcntl(tsmp.containers[i].fd, F_SETOWN_EX, (unsigned long)&fown_ex);
            if(ret)
            {
                perror("fcntl");
                return 1;
            } 
        }

        return 0;
    }

    static int begin_thread_sampling() {
        int ret;

        ret = ioctl(tsmp.containers[0].fd, PERF_EVENT_IOC_RESET, 0);
        if(ret)
            perror("ioctl");

        ret = ioctl(tsmp.containers[0].fd, PERF_EVENT_IOC_ENABLE, 0);
        if(ret)
            perror("ioctl");

    }

    static void parse_configset() {
        config = RuntimeConfig::init("perf_event", s_configdata);

        events_strvec.clear();
        std::string event_list_string = config.get("event_list").to_string();
        util::split(event_list_string, ':', back_inserter(events_strvec));
        for(auto event_str : events_strvec) {
            events.push_back(std::stoull(event_str, 0, 16));
        }

        sample_attributes_strvec.clear();
        std::string sample_attributes_string = config.get("sample_attributes").to_string();
        util::split(sample_attributes_string, ':', back_inserter(sample_attributes_strvec));
        for(auto sample_attribute_str : sample_attributes_strvec) {
            sample_attributes |= sample_attribute_map[sample_attribute_str];
        }

        sampling_frequency = config.get("frequency").to_uint();
        num_events = events.size();

    }

    void create_scope_cb(Caliper* c, cali_context_scope_t scope) {
        if (scope == CALI_SCOPE_THREAD) {
            init_thread_perf_events();
            init_thread_sighandler();
            begin_thread_sampling();
        }
    }

    void release_scope_cb(Caliper* c, cali_context_scope_t scope) {
        // TODO: end sampling, clean up
    }

    void finish_cb(Caliper* c) {
    }

    // Initialization handler
    void perf_event_register(Caliper* c) {
        config = RuntimeConfig::init("perf_event", s_configdata);

        parse_configset();
        setup_perf_event_attrs();
        
        c->events().create_scope_evt.connect(create_scope_cb);
        c->events().finish_evt.connect(finish_cb);

        Log(1).stream() << "Registered perf_event service" << endl;
    }

} // namespace


namespace cali 
{
    CaliperService perf_event_service = { "perf_event", ::perf_event_register };
} // namespace cali
