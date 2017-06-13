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

///@file  Libpfm.cpp
///@brief libpfm sampling provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>
#include <SnapshotRecord.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <ContextRecord.h>

#include <util/split.hpp>

#include <mutex>

#include <sys/time.h>
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

extern "C"
{
    #include "perf_util.h"
}

#ifndef F_SETOWN_EX
    #define F_SETOWN_EX	    15
    #define F_GETOWN_EX	    16

    #define F_OWNER_TID	    0
    #define F_OWNER_PID	    1
    #define F_OWNER_PGRP	2

    struct f_owner_ex {
        int	type;
        pid_t	pid;
    };
#endif

using namespace cali;
using namespace std;

namespace {
    ConfigSet config;

#define MAX_THR  4096
#define MAX_ATTRIBUTES 12 // Number of available attributes below

    static cali_id_t libpfm_attributes[MAX_ATTRIBUTES] = {CALI_INV_ID};
    static size_t libpfm_attribute_types[MAX_ATTRIBUTES];
    static __thread uint64_t *sample_attribute_pointers[MAX_ATTRIBUTES];

    std::map <std::string, uint64_t> sample_attribute_map = {
            {"ip",          PERF_SAMPLE_IP},
            {"id",          PERF_SAMPLE_ID},
            {"stream_id",   PERF_SAMPLE_STREAM_ID},
            {"time",        PERF_SAMPLE_TIME},
            {"tid",         PERF_SAMPLE_TID},
            {"period",      PERF_SAMPLE_PERIOD},
            {"cpu",         PERF_SAMPLE_CPU},
            {"addr",        PERF_SAMPLE_ADDR},
            {"weight",      PERF_SAMPLE_WEIGHT},
            {"transaction", PERF_SAMPLE_TRANSACTION},
            {"data_src",    PERF_SAMPLE_DATA_SRC}
    };

    static const ConfigSet::Entry s_configdata[] = {
            {"event_list", CALI_TYPE_STRING, "cycles",
             "Event List",
             "Comma-separated list of events to sample"
            },
            {"sample_attributes", CALI_TYPE_STRING, "ip,time,tid,cpu",
             "Sample attributes",
             "Comma-separated list of attributes to record for each sample"
            },
            {"period", CALI_TYPE_UINT, "20000000",
             "Sampling period",
             "Period of events until a sample is generated."
            },
            {"precise_ip", CALI_TYPE_UINT, "0",
             "Use Precise IP?",
             "Requests precise IP for supporting architectures (e.g. PEBS). May be 0, 1, or 2."
            },
            {"config1", CALI_TYPE_UINT, "0",
             "Extra event configuration",
             "Specifies extra event configuration value for supported events (e.g. PEBS latency threshold)"
            },
            ConfigSet::Terminator
    };

    /*
     * Service configuration variables
     */
    int num_attributes = 0;
    static unsigned int sampling_period;
    static unsigned int precise_ip;
    static unsigned int config1;
    static std::string events_string;
    static std::vector <uint64_t> events;

    static std::vector <std::string> sample_attributes_strvec;
    static uint64_t sample_attributes = 0;

    /*
     * libpfm sampling variables
     */

    struct thread_state {
        thread_state() {
            memset(this, 0, sizeof(thread_state));
        }

        int id;
        int fd;
        pid_t tid;
        perf_event_desc_t *fds;

        int signals_received;
        int samples_produced;
        int mismatches;
        int null_events;
        int null_cali_instances;
    };

    thread_state thread_states[MAX_THR];

    static __thread perf_event_sample_t sample;

    static __thread int thread_id;
    static __thread perf_event_desc_t *fds;
    static __thread int num_fds;

    static int signum = SIGIO;
    static int buffer_pages = 1;

    static std::mutex id_mutex;
    static int num_threads = 0;

    static pid_t gettid(void) {
        return (pid_t) syscall(__NR_gettid);
    }

    static void sample_handler() {
        Caliper c = Caliper::sigsafe_instance();

        if (!c) {
            thread_states[thread_id].null_cali_instances++;
            return;
        }

        Variant data[MAX_ATTRIBUTES];

        uint64_t *vptr;
        for (int attribute_index = 0; attribute_index < num_attributes; attribute_index++) {
            vptr = sample_attribute_pointers[attribute_index];
            data[attribute_index] = Variant(static_cast<uint64_t>(*vptr));
        }

        SnapshotRecord trigger_info(num_attributes, libpfm_attributes, data);

        c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);

        thread_states[thread_id].samples_produced++;
    }

    static void sigio_handler(int sig, siginfo_t *info, void *extra) {
        perf_event_desc_t *fdx;
        struct perf_event_header ehdr;
        thread_state *ts;
        int fd, i, ret;
        bool skip = false;
        pid_t tid;

        ret = ioctl(thread_states[thread_id].fd, PERF_EVENT_IOC_DISABLE, 0);
        if (ret)
            err(1, "cannot stop sampling for handling");

        thread_states[thread_id].signals_received++;

        fd = info->si_fd;
        tid = gettid();

        for (i = 0; i < MAX_THR; i++) {
            if (thread_states[i].fd == fd) {
                break;
            }
        }

        if (i == MAX_THR) {
            warnx("bad info.si_fd: %d NEQ %d", fd, thread_states[thread_id].fd);
        }

        ts = &thread_states[i];

        if (tid != ts->tid) {
            thread_states[thread_id].mismatches++;
            fdx = ts->fds;
            skip = true;
        } else {
            fdx = fds;
        }

        if (fdx) {
            // Read header
            ret = perf_read_buffer(fdx + 0, &ehdr, sizeof(ehdr));
            if (ret) {
                warnx("cannot read event header");
            }

            if (skip) {
                // Consume only (don't record a different threads data on this thread)
                perf_skip_buffer(fdx + 0, ehdr.size);
            } else {
                // Read and record
                //ret = perf_display_sample(fdx, 1, fdx->id, &ehdr, stderr);
                ret = perf_read_sample(fdx, 1, fdx->id, &ehdr, &sample);
                if (ret) {
                    warnx("cannot read sample");
                }

                // fprintf(stderr, "CPU: %d, IP: %d PID: %d TID: %d TIME: %d\n",
                //     sample.cpu, sample.ip, sample.pid, sample.tid, sample.time);
                sample_handler();
            }
        } else {
            thread_states[thread_id].null_events++;
        }

        // Reset
        ret = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
        if (ret) {
            err(1, "cannot refresh");
        }
    }

    static void setup_thread_events() {
        struct thread_state *ts;
        struct f_owner_ex fown_ex;
        int ret, fd, flags;
        size_t pgsz = sysconf(_SC_PAGESIZE);

        // Get perf_event from string
        fds = NULL;
        num_fds = 0;
        ret = perf_setup_list_events(events_string.c_str(), &fds, &num_fds);
        if (ret || !num_fds)
            errx(1, "cannot monitor event");

        // Set up perf_event
        fds[0].hw.disabled = 1;
        fds[0].hw.wakeup_events = 1;
        fds[0].hw.sample_type = sample_attributes;
        fds[0].hw.sample_period = sampling_period;
        fds[0].hw.read_format = 0;
        fds[0].hw.precise_ip = precise_ip;
        fds[0].hw.config1 = config1;

        fds[0].fd = fd = perf_event_open(&fds[0].hw, gettid(), -1, -1, 0);
        if (fd == -1)
            err(1, "cannot attach event %s", fds[0].name);

        // Get unique thread id
        id_mutex.lock();
        thread_id = num_threads++;
        id_mutex.unlock();

        // Set thread state
        ts = &thread_states[thread_id];
        ts->id = thread_id;
        ts->fd = fd;
        ts->tid = gettid();
        ts->fds = fds;

        // Set up perf_event file descriptor to signal this thread
        flags = fcntl(fd, F_GETFL, 0);
        if (fcntl(fd, F_SETFL, flags | O_ASYNC) < 0)
            err(1, "fcntl SETFL failed");

        fown_ex.type = F_OWNER_TID;
        fown_ex.pid = gettid();
        ret = fcntl(fd, F_SETOWN_EX, (unsigned long) &fown_ex);
        if (ret)
            err(1, "fcntl SETOWN failed");

        if (fcntl(fd, F_SETSIG, signum) < 0)
            err(1, "fcntl SETSIG failed");

        // Create mmap buffer for samples
        fds[0].buf = mmap(NULL, (buffer_pages + 1) * pgsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (fds[0].buf == MAP_FAILED)
            err(1, "cannot mmap buffer");

        fds[0].pgmsk = (buffer_pages * pgsz) - 1;
    }

    static int setup_process_signals() {
        struct sigaction sa;
        sigset_t set, oldsig, newsig;
        int ret, i;

        memset(&sa, 0, sizeof(sa));
        sigemptyset(&set);

        sa.sa_sigaction = sigio_handler;
        sa.sa_mask = set;
        sa.sa_flags = SA_SIGINFO;

        if (sigaction(signum, &sa, NULL) != 0)
            errx(1, "sigaction failed");

        if (pfm_initialize() != PFM_SUCCESS)
            errx(1, "pfm_initialize failed");

        sigemptyset(&set);
        sigemptyset(&newsig);
        sigaddset(&set, SIGIO);
        sigaddset(&newsig, SIGIO);

        if (pthread_sigmask(SIG_BLOCK, &set, NULL))
            err(1, "cannot mask SIGIO in main thread");

        ret = sigprocmask(SIG_SETMASK, NULL, &oldsig);
        if (ret)
            err(1, "sigprocmask failed");

        if (sigismember(&oldsig, SIGIO)) {
            warnx("program started with SIGIO masked, unmasking it now\n");
            ret = sigprocmask(SIG_UNBLOCK, &newsig, NULL);
            if (ret)
                err(1, "sigprocmask failed");
        }

        return ret;
    }

    static int begin_thread_sampling() {
        int ret = ioctl(fds[0].fd, PERF_EVENT_IOC_REFRESH, 1);

        if (ret == -1)
            err(1, "cannot refresh");

        return ret;
    }

    static int end_thread_sampling() {
        int ret = ioctl(thread_states[thread_id].fd, PERF_EVENT_IOC_DISABLE, 0);

        if (ret)
            err(1, "cannot stop");

        return ret;
    }

    static void parse_configset(Caliper *c) {
        config = RuntimeConfig::init("libpfm", s_configdata);

        events_string = config.get("event_list").to_string();

        num_attributes = 0;
        sample_attributes_strvec.clear();
        std::string sample_attributes_string = config.get("sample_attributes").to_string();
        util::split(sample_attributes_string, ',', back_inserter(sample_attributes_strvec));

        for (auto sample_attribute_str : sample_attributes_strvec) {

            std::string attribute_name = "libpfm." + sample_attribute_str;

            // Add to sample attributes for perf_event
            uint64_t attribute_bits = sample_attribute_map[sample_attribute_str];
            sample_attributes |= attribute_bits;

            Attribute new_attribute;

            if (attribute_bits == PERF_SAMPLE_IP) {

                // Register IP attribute for symbol lookup
                Attribute symbol_class_attr = c->get_attribute("class.symboladdress");
                Variant v_true(true);

                new_attribute = c->create_attribute(attribute_name,
                                                    CALI_TYPE_ADDR,
                                                    CALI_ATTR_ASVALUE
                                                    | CALI_ATTR_SCOPE_THREAD
                                                    | CALI_ATTR_SKIP_EVENTS,
                                                    1, &symbol_class_attr, &v_true);
            } else {
                new_attribute = c->create_attribute(attribute_name,
                                                    CALI_TYPE_ADDR,
                                                    CALI_ATTR_ASVALUE
                                                    | CALI_ATTR_SCOPE_THREAD
                                                    | CALI_ATTR_SKIP_EVENTS);
            }

            // Add to attribute ids
            cali_id_t attribute_id = new_attribute.id();
            libpfm_attributes[num_attributes] = attribute_id;

            // Record type of attribute
            libpfm_attribute_types[num_attributes] = attribute_bits;

            ++num_attributes;
        }

        sampling_period = config.get("period").to_uint();

        precise_ip = config.get("precise_ip").to_uint();

        config1 = config.get("config1").to_uint();
    }

    static void setup_thread_pointers() {

        uint64_t value;
        cali_id_t attribute_id;
        size_t attribute_type;
        for (int attribute_index = 0; attribute_index < num_attributes; attribute_index++) {

            attribute_id = libpfm_attributes[attribute_index];
            attribute_type = libpfm_attribute_types[attribute_index];

            switch (attribute_type) {

                case (PERF_SAMPLE_IP):
                    sample_attribute_pointers[attribute_index] = &sample.ip;
                    break;
                case (PERF_SAMPLE_ID):
                    sample_attribute_pointers[attribute_index] = &sample.id;
                    break;
                case (PERF_SAMPLE_STREAM_ID):
                    sample_attribute_pointers[attribute_index] = &sample.stream_id;
                    break;
                case (PERF_SAMPLE_TIME):
                    sample_attribute_pointers[attribute_index] = &sample.time;
                    break;
                case (PERF_SAMPLE_TID):
                    sample_attribute_pointers[attribute_index] = &sample.tid;
                    break;
                case (PERF_SAMPLE_PERIOD):
                    sample_attribute_pointers[attribute_index] = &sample.period;
                    break;
                case (PERF_SAMPLE_CPU):
                    sample_attribute_pointers[attribute_index] = &sample.cpu;
                    break;
                case (PERF_SAMPLE_ADDR):
                    sample_attribute_pointers[attribute_index] = &sample.addr;
                    break;
                case (PERF_SAMPLE_WEIGHT):
                    sample_attribute_pointers[attribute_index] = &sample.weight;
                    break;
                case (PERF_SAMPLE_TRANSACTION):
                    sample_attribute_pointers[attribute_index] = &sample.transaction;
                    break;
                case (PERF_SAMPLE_DATA_SRC):
                    sample_attribute_pointers[attribute_index] = &sample.data_src;
                    break;
                default:
                    errx(1, "Attribute unrecognized!");
                    return;
            }
        }
    }

    void post_init_cb(Caliper* c) {
        // Run on master thread initialization
        setup_thread_events();
        setup_thread_pointers();
        begin_thread_sampling();
    }

    void create_scope_cb(Caliper* c, cali_context_scope_t scope) {
        if (scope == CALI_SCOPE_THREAD) {
            setup_thread_events();
            setup_thread_pointers();
            begin_thread_sampling();
        }
    }

    void release_scope_cb(Caliper* c, cali_context_scope_t scope) {
        if (scope == CALI_SCOPE_THREAD) {
            end_thread_sampling();
        }
    }

    void finish_cb(Caliper* c) {
        pfm_terminate();

        Log(1).stream() << "libpfm thread stats:" << std::endl;
        for (int i=0; i<num_threads; i++) {
            Log(1).stream() << "thread " << i
                            << "\tsignals received: " << thread_states[i].signals_received
                            << "\tsamples produced: " << thread_states[i].samples_produced
                            << "\tmismatches: " << thread_states[i].mismatches
                            << "\tnull events: " << thread_states[i].null_events
                            << "\tnull cali instances: " << thread_states[i].null_cali_instances << std::endl;
        }
    }

    // Initialization handler
    void libpfm_service_register(Caliper* c) {
        config = RuntimeConfig::init("libpfm", s_configdata);

        parse_configset(c);
        setup_process_signals();
        
        c->events().create_scope_evt.connect(create_scope_cb);
        c->events().post_init_evt.connect(post_init_cb);
        c->events().finish_evt.connect(finish_cb);

        Log(1).stream() << "Registered libpfm service" << endl;
    }

} // namespace


namespace cali 
{
    CaliperService libpfm_service = { "libpfm", ::libpfm_service_register };
} // namespace cali
