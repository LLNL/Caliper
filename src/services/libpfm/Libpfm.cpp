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

namespace 
{
    Attribute dummy_attr      { Attribute::invalid } ;

    ConfigSet config;

#define MAX_ATTRIBUTES 12 // Number of available attributes below

    cali_id_t libpfm_attributes[MAX_ATTRIBUTES] = { CALI_INV_ID };

    int num_samples = 0;
    int num_processed_samples = 0;
    static __thread perf_event_sample_t sample;

    std::map<std::string, uint64_t> sample_attribute_map = {
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

    std::map<uint64_t, size_t> attribute_offset_map = {
            {PERF_SAMPLE_IP,            (uint64_t*)&(sample.ip)          - (uint64_t*)&sample},
            {PERF_SAMPLE_ID,            (uint64_t*)&(sample.sample_id)   - (uint64_t*)&sample},
            {PERF_SAMPLE_STREAM_ID,     (uint64_t*)&(sample.stream_id)   - (uint64_t*)&sample},
            {PERF_SAMPLE_TIME,          (uint64_t*)&(sample.time)        - (uint64_t*)&sample},
            {PERF_SAMPLE_TID,           (uint64_t*)&(sample.tid)         - (uint64_t*)&sample},
            {PERF_SAMPLE_PERIOD,        (uint64_t*)&(sample.period)      - (uint64_t*)&sample},
            {PERF_SAMPLE_CPU,           (uint64_t*)&(sample.cpu)         - (uint64_t*)&sample},
            {PERF_SAMPLE_ADDR,          (uint64_t*)&(sample.addr)        - (uint64_t*)&sample},
            {PERF_SAMPLE_WEIGHT,        (uint64_t*)&(sample.weight)      - (uint64_t*)&sample},
            {PERF_SAMPLE_TRANSACTION,   (uint64_t*)&(sample.transaction) - (uint64_t*)&sample},
            {PERF_SAMPLE_DATA_SRC,      (uint64_t*)&(sample.data_src)    - (uint64_t*)&sample}
    };

    std::map<cali_id_t, size_t> active_attribute_offset_map;

    static const ConfigSet::Entry s_configdata[] = {
        { "event_list", CALI_TYPE_STRING, "cycles",
          "Event List",
          "Comma-separated list of events to sample"
        },
        { "sample_attributes", CALI_TYPE_STRING, "ip,time,tid,cpu",
          "Sample attributes",
          "Comma-separated list of attributes to record for each sample"
        },
        { "frequency", CALI_TYPE_UINT, "4000",
          "Sampling frequency",
          "Number of samples per second to collect (approximately)."
        },
        { "precise_ip", CALI_TYPE_UINT, "0",
          "Use Precise IP?",
          "Requests precise IP for supporting architectures (e.g. PEBS). May be 0, 1, or 2."
        },
        ConfigSet::Terminator
    };

    /*
     * Service configuration variables
     */
    int num_attributes = 0;
    static unsigned int sampling_frequency;
    static unsigned int precise_ip;
    static std::string events_string;
    static std::vector<uint64_t> events;

    static std::vector<std::string> sample_attributes_strvec;
    static uint64_t sample_attributes = 0;

    /*
     * libpfm sampling variables
     */

    struct over_args {
        int fd;
        pid_t tid;
        perf_event_desc_t *fds;
    };

    std::map<int, over_args> fdmap;

    static int signum = SIGIO;
    static int buffer_pages = 1;
    int fown;

    static __thread int myid;
    static __thread perf_event_desc_t *fds;
    static __thread int num_fds;

    static pid_t gettid(void)
    {
        return (pid_t)syscall(__NR_gettid);
    }

    static void sigusr1_handler(int sig, siginfo_t *info, void *context)
    {
    }

    static void sample_handler()
    {
        Caliper c = Caliper::sigsafe_instance();

        if (!c)
            return;

        Variant data[MAX_ATTRIBUTES];

        int attribute_index = 0;
        for(auto attribute_id : libpfm_attributes)
        {
            size_t offset = active_attribute_offset_map[attribute_id];
            uint64_t value = ((uint64_t*)&sample)[offset];
            data[attribute_index] = Variant(static_cast<uint64_t>(value));
        };

        SnapshotRecord trigger_info(num_attributes, libpfm_attributes, data);

        c.push_snapshot(CALI_SCOPE_THREAD, &trigger_info);
    }

    static void sigio_handler(int sig, siginfo_t *info, void *extra)
    {
        perf_event_desc_t *fdx;
        struct perf_event_header ehdr;
        over_args *ov;
        int fd, i, ret;
        pid_t tid;

        ++num_samples;

        // Get the perf_event file descriptor and thread
        fd = info->si_fd;
        tid = gettid();

        ov = &fdmap[fd];

        if (tid != ov->tid) {
            // mismatch
            fdx = ov->fds;
        } else {
            fdx = fds;
        }

        if (fdx) {

            // Read header
            ret = perf_read_buffer(fdx + 0, &ehdr, sizeof(ehdr));
            if (ret) {
                errx(1, "cannot read event header");
            }

            // Read sample
            ret = perf_read_sample(fdx, 1, 0, &ehdr, &sample);
            if (ret) {
                errx(1, "cannot read sample");
            }

            sample_handler();

            ++num_processed_samples;
        }


        // Reset
        ret = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
        if (ret)
            err(1, "cannot refresh");
    }

    /*
     * Set up signals for thread
     */
    static void setup_thread() {
        struct f_owner_ex fown_ex;
        size_t pgsz;
        int ret, fd, flags;

        fds = NULL;
        num_fds = 0;
        ret = perf_setup_list_events(events_string.c_str(), &fds, &num_fds);
        if (ret || !num_fds)
            errx(1, "cannot monitor event");

        pgsz = sysconf(_SC_PAGESIZE);

        /* do not enable now */
        fds[0].hw.disabled = 1;

        /* notify after 1 sample */
        fds[0].hw.wakeup_events = 1;
        fds[0].hw.sample_type = sample_attributes;
        fds[0].hw.sample_freq = sampling_frequency;
        fds[0].hw.read_format = 0;
        fds[0].hw.precise_ip = precise_ip;

        fds[0].fd = fd = perf_event_open(&fds[0].hw, gettid(), -1, -1, 0);
        if (fd == -1)
            err(1, "cannot attach event %s", fds[0].name);

        struct over_args ov = {fd, gettid(), fds};
        fdmap[fd] = ov;

        flags = fcntl(fd, F_GETFL, 0);
        if (fcntl(fd, F_SETFL, flags | O_ASYNC) < 0)
            err(1, "fcntl SETFL failed");

        fown_ex.type = F_OWNER_TID;
        fown_ex.pid  = gettid();
        ret = fcntl(fd,
                    (fown ? F_SETOWN_EX : F_SETOWN),
                    (fown ? (unsigned long)&fown_ex: (unsigned long)gettid()));
        if (ret)
            err(1, "fcntl SETOWN failed");

        if (fcntl(fd, F_SETSIG, signum) < 0)
            err(1, "fcntl SETSIG failed");

        fds[0].buf = mmap(NULL, (buffer_pages + 1)* pgsz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (fds[0].buf == MAP_FAILED)
            err(1, "cannot mmap buffer");

        fds[0].pgmsk = (buffer_pages * pgsz) - 1;
    }

    static int setup_signals() {
        struct sigaction sa;
        sigset_t set, oldsig, newsig;
        int ret, i;

        memset(&sa, 0, sizeof(sa));
        sigemptyset(&set);

        sa.sa_sigaction = sigusr1_handler;
        sa.sa_mask = set;
        sa.sa_flags = SA_SIGINFO;

        if (sigaction(SIGUSR1, &sa, NULL) != 0)
            errx(1, "sigaction failed");

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
    }

    static int begin_thread_sampling() {
        int ret = ioctl(fds[0].fd, PERF_EVENT_IOC_REFRESH , 1);
        if (ret == -1)
            err(1, "cannot refresh");
    }

    static void parse_configset(Caliper* c) {
        config = RuntimeConfig::init("libpfm", s_configdata);

        events_string = config.get("event_list").to_string();

        num_attributes = 0;
        sample_attributes_strvec.clear();
        std::string sample_attributes_string = config.get("sample_attributes").to_string();
        util::split(sample_attributes_string, ',', back_inserter(sample_attributes_strvec));

        for(auto sample_attribute_str : sample_attributes_strvec) {

            // Create Caliper attribute
            std::string attribute_name = "libpfm." + sample_attribute_str;

            Attribute new_attribute = c->create_attribute(attribute_name,
                                                          CALI_TYPE_ADDR,
                                                          CALI_ATTR_ASVALUE
                                                          | CALI_ATTR_SCOPE_THREAD
                                                          | CALI_ATTR_SKIP_EVENTS);

            // Add to attribute ids
            cali_id_t attribute_id = new_attribute.id();
            libpfm_attributes[num_attributes] = attribute_id;

            // Add to sample attributes for perf_event
            uint64_t attribute_bits = sample_attribute_map[sample_attribute_str];
            sample_attributes |= attribute_bits;

            // Map id to offset
            active_attribute_offset_map[attribute_id] = attribute_offset_map[attribute_bits];

            ++num_attributes;
        }

        sampling_frequency = config.get("frequency").to_uint();

        precise_ip = config.get("precise_ip").to_uint();
    }

    void create_scope_cb(Caliper* c, cali_context_scope_t scope) {
        if (scope == CALI_SCOPE_THREAD) {
            setup_thread();
            begin_thread_sampling();
        }
    }

    void release_scope_cb(Caliper* c, cali_context_scope_t scope) {
        // TODO: how to stop sampling on this particular thread?
        // int ret = ioctl(???, PERF_EVENT_IOC_DISABLE, 0);
        // if (ret)
            // err(1, "cannot stop");
    }

    void finish_cb(Caliper* c) {
        pfm_terminate();

        Log(1).stream() << "libpfm: processed " << num_processed_samples
                        << " samples ("  << num_samples
                        << " total, "    << num_samples - num_processed_samples
                        << " dropped)."  << std::endl;
    }

    // Initialization handler
    void libpfm_service_register(Caliper* c) {
        config = RuntimeConfig::init("libpfm", s_configdata);

        parse_configset(c);
        setup_signals();
        
        c->events().create_scope_evt.connect(create_scope_cb);
        c->events().finish_evt.connect(finish_cb);

        Log(1).stream() << "Registered libpfm service" << endl;
    }

} // namespace


namespace cali 
{
    CaliperService libpfm_service = { "libpfm", ::libpfm_service_register };
} // namespace cali
