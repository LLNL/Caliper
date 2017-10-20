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

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/ContextRecord.h"

#include "caliper/common/util/split.hpp"

#include <mutex>

#include <sys/time.h>
#include <sys/types.h>
#include <inttypes.h>
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
//#include <linux/perf_event.h>

#include "perf_postprocessing.h"

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
#define MAX_EVENTS 12 // Maximum number of events to count/sample
#define MAX_ATTRIBUTES 12 // Number of available attributes below

    static cali_id_t libpfm_attributes[MAX_ATTRIBUTES] = {CALI_INV_ID};
    static Attribute libpfm_event_name_attr;
    static cali_id_t libpfm_event_name_attr_id = {CALI_INV_ID};
    static size_t libpfm_attribute_types[MAX_ATTRIBUTES];
    static std::map<size_t, Attribute> libpfm_attribute_type_to_attr;
    static __thread uint64_t *sample_attribute_pointers[MAX_ATTRIBUTES];
    static std::vector<Node*> event_name_nodes;

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
            {"events", CALI_TYPE_STRING, "cycles",
             "Event list",
             "Comma-separated list of events to sample"
            },
            {"record_counter", CALI_TYPE_BOOL, "true",
             "Record counter values (true|false)",   
             "Whether to record event counter values at each snapshot (true|false)"
            },
            {"enable_sampling", CALI_TYPE_BOOL, "true",
             "Enable sampling",   
             "Whether to trigger and record samples"
            },
            {"sample_attributes", CALI_TYPE_STRING, "ip,time,tid,cpu",
             "Sample attributes",
             "Comma-separated list of attributes to record for each sample"
            },
            {"sample_period", CALI_TYPE_UINT, "20000000",
             "Event sampling periods",
             "Comma-separated list of event periods"
            },
            {"precise_ip", CALI_TYPE_STRING, "0",
             "Precise IP values for events",
             "Comma-separated list of precise IP values for respective events"
            },
            {"config1", CALI_TYPE_STRING, "0",
             "Extra event configurations",
             "Comma-separated list of extra event configuration values for supported events"
            },
            ConfigSet::Terminator
    };

    /*
     * Service configuration variables
     */
    int num_attributes = 0;
    static unsigned int record_counter;
    static std::string events_string;
    static std::vector<uint64_t> sampling_period_list;
    static std::vector<uint64_t> precise_ip_list;
    static std::vector<uint64_t> config1_list;

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
        pid_t tid;
        perf_event_desc_t *fds;

        int signals_received;
        int samples_produced;
        int null_events;
        int null_cali_instances;
    };

    thread_state thread_states[MAX_THR];

    static __thread perf_event_sample_t sample;

    static __thread int thread_id;
    static __thread perf_event_desc_t *fds;
    static __thread int num_events;

    static int signum = SIGIO;
    static int buffer_pages = 1;

    static std::mutex id_mutex;
    static int num_threads = 0;

    static pid_t gettid(void) {
        return (pid_t) syscall(__NR_gettid);
    }

    static void sample_handler(int event_index) {
        Caliper c = Caliper::sigsafe_instance();

        if (!c) {
            thread_states[thread_id].null_cali_instances++;
            return;
        }

        Variant data[MAX_ATTRIBUTES];

        uint64_t *vptr;
        for (int attribute_index = 0; attribute_index < num_attributes; attribute_index++) {
            vptr = sample_attribute_pointers[attribute_index];
            data[attribute_index] = Variant(*vptr);
        }

        SnapshotRecord trigger_info(1, &event_name_nodes[event_index], num_attributes, libpfm_attributes, data);

        c.push_snapshot(CALI_SCOPE_PROCESS | CALI_SCOPE_THREAD, &trigger_info);

        thread_states[thread_id].samples_produced++;
    }

    static void sigio_handler(int sig, siginfo_t *info, void *extra) {
        perf_event_desc_t *fdx = 0;
        struct perf_event_header ehdr;
        int fd, i, ret;

        fd = info->si_fd;

        ret = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        if (ret)
            Log(0).stream() << "libpfm: cannot stop sampling for handling" << std::endl;

        thread_states[thread_id].signals_received++;

        for (i=0; i<num_events; i++) {
            if (fd == fds[i].fd) {
                fdx = &fds[i];
                break;
            }
        }

        if (fdx) {
            // Read header
            ret = perf_read_buffer(fdx, &ehdr, sizeof(ehdr));
            if (ret) {
                Log(1).stream() << "libpfm: cannot read event header" << std::endl;
            }

            // Read and record
            ret = perf_read_sample(fdx, 1, 0, &ehdr, &sample, stderr); // any way to print to Log(1)?
            if (ret) {
                Log(1).stream() << "libpfm: cannot read sample" << std::endl;
            }

            // Run handler to push snapshot
            sample_handler(i);
        } else {
            thread_states[thread_id].null_events++;
        }

        // Reset
        ret = ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
        if (ret) {
            Log(0).stream() << "libpfm: unable to refresh sampling!" << std::endl;
        }
    }

    static void setup_thread_events(Caliper *c) {
        struct thread_state *ts;
        struct f_owner_ex fown_ex;
        int ret, fd, flags, i;
        size_t pgsz = sysconf(_SC_PAGESIZE);

        // Get unique thread id
        id_mutex.lock();
        thread_id = num_threads++;
        id_mutex.unlock();

        // Set thread state
        ts = &thread_states[thread_id];
        ts->id = thread_id;
        ts->tid = gettid();
        ts->fds = fds;

        // Get perf_event from string
        fds = NULL;
        num_events = 0;
        ret = perf_setup_list_events(events_string.c_str(), &fds, &num_events);
        if (ret || !num_events)
            Log(0).stream() << "libpfm: invalid event(s) specified!" << std::endl;
        
        if (num_events > MAX_EVENTS)
            Log(0).stream() << "libpfm: too many events specified for libpfm service! Maximum is " << MAX_EVENTS << std::endl;

        fds[0].fd = -1;
        for(i=0; i < num_events; i++) {

            // Set up perf_event
            fds[i].hw.disabled = (i == 0) ? 1 : 0;
            fds[i].hw.wakeup_events = 1;
            fds[i].hw.sample_type = sample_attributes;
            fds[i].hw.sample_period = sampling_period_list[i];
            fds[i].hw.read_format = 0;
            fds[i].hw.precise_ip = precise_ip_list[i];
            fds[i].hw.config1 = config1_list[i];

            fds[i].fd = fd = perf_event_open(&fds[i].hw, ts->tid, -1, fds[0].fd, 0);
            if (fd == -1)
                Log(0).stream() << "libpfm: cannot attach event " << fds[i].name << std::endl;

            // Set up perf_event file descriptor to signal this thread
            flags = fcntl(fd, F_GETFL, 0);
            if (fcntl(fd, F_SETFL, flags | O_ASYNC) < 0)
                Log(0).stream() << "libpfm: fcntl SETFL failed" << std::endl;

            fown_ex.type = F_OWNER_TID;
            fown_ex.pid = ts->tid;
            ret = fcntl(fd, F_SETOWN_EX, (unsigned long) &fown_ex);
            if (ret)
                Log(0).stream() << "libpfm: fcntl SETOWN failed" << std::endl;

            if (fcntl(fd, F_SETSIG, signum) < 0)
                Log(0).stream() << "libpfm: fcntl SETSIG failed" << std::endl;

            // Create mmap buffer for samples
            fds[i].buf = mmap(NULL, (buffer_pages + 1) * pgsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (fds[i].buf == MAP_FAILED)
                Log(0).stream() << "libpfm: cannot mmap buffer for event " << fds[i].name << std::endl;

            fds[i].pgmsk = (buffer_pages * pgsz) - 1;

            // Store Caliper nodes for each event name
            event_name_nodes.push_back(c->make_tree_entry(libpfm_event_name_attr,
                                                          Variant(CALI_TYPE_STRING, fds[i].name, strlen(fds[i].name))));
        }
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
            Log(0).stream() << "libpfm: sigaction failed" << std::endl;

        if (pfm_initialize() != PFM_SUCCESS)
            Log(0).stream() << "libpfm: pfm_initialize failed" << std::endl;

        sigemptyset(&set);
        sigemptyset(&newsig);
        sigaddset(&set, SIGIO);
        sigaddset(&newsig, SIGIO);

        if (pthread_sigmask(SIG_BLOCK, &set, NULL))
            Log(0).stream() << "libpfm: cannot mask SIGIO in main thread" << std::endl;

        ret = sigprocmask(SIG_SETMASK, NULL, &oldsig);
        if (ret)
            Log(0).stream() << "libpfm: sigprocmask failed" << std::endl;

        if (sigismember(&oldsig, SIGIO)) {
            Log(1).stream() << "libpfm: program started with SIGIO masked, unmasking it now" << std::endl;
            ret = sigprocmask(SIG_UNBLOCK, &newsig, NULL);
            if (ret)
                Log(0).stream() << "libpfm: sigprocmask failed" << std::endl;
        }

        return ret;
    }

    static int begin_thread_sampling() {
        int ret = ioctl(fds[0].fd, PERF_EVENT_IOC_REFRESH, 1);

        if (ret == -1)
            Log(0).stream() << "libpfm: cannot refresh sampler" << std::endl;

        return ret;
    }

    static int end_thread_sampling() {
        int ret = ioctl(fds[0].fd, PERF_EVENT_IOC_DISABLE, 0);

        if (ret)
            Log(0).stream() <<  "libpfm: cannot stop sampling" << std::endl;

        return ret;
    }

    static bool parse_configset(Caliper *c) {
        config = RuntimeConfig::init("libpfm", s_configdata);

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
                                                    CALI_TYPE_UINT,
                                                    CALI_ATTR_ASVALUE
                                                    | CALI_ATTR_SCOPE_THREAD
                                                    | CALI_ATTR_SKIP_EVENTS,
                                                    1, &symbol_class_attr, &v_true);
            } else if (attribute_bits == PERF_SAMPLE_ADDR) {

                // Register ADDR attribute for memory address lookup
                Attribute memory_class_attr = c->get_attribute("class.memoryaddress");
                Variant v_true(true);

                new_attribute = c->create_attribute(attribute_name,
                                                    CALI_TYPE_UINT,
                                                    CALI_ATTR_ASVALUE
                                                    | CALI_ATTR_SCOPE_THREAD
                                                    | CALI_ATTR_SKIP_EVENTS,
                                                    1, &memory_class_attr, &v_true);
            } else {

                new_attribute = c->create_attribute(attribute_name,
                                                    CALI_TYPE_UINT,
                                                    CALI_ATTR_ASVALUE
                                                    | CALI_ATTR_SCOPE_THREAD
                                                    | CALI_ATTR_SKIP_EVENTS);
            }

            // Add to attribute ids
            cali_id_t attribute_id = new_attribute.id();
            libpfm_attributes[num_attributes] = attribute_id;

            // Record type of attribute
            libpfm_attribute_types[num_attributes] = attribute_bits;

            // Map type to id for postprocessing
            libpfm_attribute_type_to_attr[attribute_bits] = new_attribute;

            ++num_attributes;
        }

        record_counter = config.get("record_counter").to_bool();

        events_string = config.get("events").to_string();

        std::string sampling_period_list_str = config.get("sample_period").to_string();
        std::string precise_ip_list_str = config.get("precise_ip").to_string();
        std::string config1_list_str = config.get("config1").to_string();

        std::vector<std::string> sampling_period_strvec;
        std::vector<std::string> precise_ip_strvec;
        std::vector<std::string> config1_strvec;

        util::split(sampling_period_list_str, ',', back_inserter(sampling_period_strvec));
        util::split(precise_ip_list_str, ',', back_inserter(precise_ip_strvec));
        util::split(config1_list_str, ',', back_inserter(config1_strvec));

        size_t events_listed = 1+std::count(events_string.begin(), events_string.end(), ',');

        if (events_listed != sampling_period_strvec.size()
            | events_listed != precise_ip_strvec.size()
            | events_listed != config1_strvec.size()) {
            
            Log(0).stream() << "libpfm: invalid arguments specified!" << std::endl;
            Log(0).stream() << "libpfm: Event list, sampling period, precise IP, and config1 must all have the same number of values." << std::endl;

            return false;
        }

        for (int i=0; i<events_listed; i++) {
            try {
                sampling_period_list.push_back(std::stoull(sampling_period_strvec[i]));
                precise_ip_list.push_back(std::stoull(precise_ip_strvec[i]));
                config1_list.push_back(std::stoull(config1_strvec[i]));
            } catch (...) {
                Log(0).stream() << "libpfm: invalid arguments specified to libpfm service!" << std::endl;
                Log(0).stream() << "libpfm: sampling period, precise IP, and config1 must be unsigned integers (uint64_t)" << std::endl;
                return false;
            }
        }

        return true;

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
                    Log(0).stream() << "libpfm: attribute unrecognized!" << std::endl;
                    return;
            }
        }
    }

    void snapshot_cb(Caliper* c, int scope, const SnapshotRecord*, SnapshotRecord* snapshot) {

        Variant data[MAX_EVENTS];

        for (int i=0; i<num_events; i++) {
            // TODO: this
            // 1. create libpfm.counter.EVENT_NAME for each event at init
            //   - make it of class "aggregatable"
            // 2. read each event counter here and record it
        }
    }

    void post_init_cb(Caliper* c) {
        // Run on master thread initialization
        setup_thread_events(c);
        setup_thread_pointers();
        begin_thread_sampling();
    }

    void create_scope_cb(Caliper* c, cali_context_scope_t scope) {
        if (scope == CALI_SCOPE_THREAD) {
            setup_thread_events(c);
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
        end_thread_sampling();

        pfm_terminate();

        Log(1).stream() << "libpfm: thread stats:" << std::endl;
        for (int i=0; i<num_threads; i++) {
            Log(1).stream() << "libpfm: thread " << i
                            << "\tsignals received: " << thread_states[i].signals_received
                            << "\tsamples produced: " << thread_states[i].samples_produced
                            << "\tunknown events: " << thread_states[i].null_events
                            << "\tnull Caliper instances: " << thread_states[i].null_cali_instances << std::endl;
        }
    }

    struct DataSrcAttrs {
        Attribute mem_lvl_attr;
        Attribute mem_hit_attr;
        Attribute mem_op_attr;
        Attribute mem_snoop_attr;
        Attribute mem_tlb_attr;
    };

    struct DataSrcAttrs data_src_attrs;

    static void pre_flush_cb(Caliper* c, const SnapshotRecord*) {
        if (sample_attributes & PERF_SAMPLE_DATA_SRC) {
            data_src_attrs.mem_lvl_attr = c->create_attribute("libpfm.memory_level",
                                                              CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
            data_src_attrs.mem_hit_attr = c->create_attribute("libpfm.hit_type",
                                                              CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
            data_src_attrs.mem_op_attr = c->create_attribute("libpfm.operation",
                                                              CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
            data_src_attrs.mem_snoop_attr = c->create_attribute("libpfm.snoop",
                                                              CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
            data_src_attrs.mem_tlb_attr = c->create_attribute("libpfm.tlb",
                                                              CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        }
    }

    static void postprocess_snapshot_cb(Caliper* c, SnapshotRecord* snapshot) {

        std::vector<Attribute> attr;
        std::vector<Variant>   data;

        std::string mem_lvl;
        std::string hit;
        std::string op;
        std::string snoop;
        std::string tlb;

        // Decode data_src encoding
        if (sample_attributes & PERF_SAMPLE_DATA_SRC) {

            Entry e = snapshot->get(libpfm_attribute_type_to_attr[PERF_SAMPLE_DATA_SRC]);

            if (!e.is_empty()) {
                uint64_t data_src = e.value().to_uint();

                mem_lvl = datasource_mem_lvl(data_src);
                hit = datasource_mem_hit(data_src);
                op = datasource_mem_op(data_src);
                snoop = datasource_mem_snoop(data_src);
                tlb = datasource_mem_tlb(data_src);

                attr.push_back(data_src_attrs.mem_lvl_attr);
                attr.push_back(data_src_attrs.mem_hit_attr);
                attr.push_back(data_src_attrs.mem_op_attr);
                attr.push_back(data_src_attrs.mem_snoop_attr);
                attr.push_back(data_src_attrs.mem_tlb_attr);

                data.push_back(Variant(CALI_TYPE_STRING, mem_lvl.c_str(), mem_lvl.size()));
                data.push_back(Variant(CALI_TYPE_STRING, hit.c_str(), hit.size()));
                data.push_back(Variant(CALI_TYPE_STRING, op.c_str(), op.size()));
                data.push_back(Variant(CALI_TYPE_STRING, snoop.c_str(), snoop.size()));
                data.push_back(Variant(CALI_TYPE_STRING, tlb.c_str(), tlb.size()));
            }
        }

        if (attr.size() > 0)
            c->make_entrylist(attr.size(), attr.data(), data.data(), *snapshot);
    }

    // Initialization handler
    void libpfm_service_register(Caliper* c) {

        if (!parse_configset(c)) {
            Log(0).stream() << "Failed to register libpfm service!" << std::endl;
            return;
        }

        config = RuntimeConfig::init("libpfm", s_configdata);

        libpfm_event_name_attr = c->create_attribute("libpfm.event_name",
                                                     CALI_TYPE_STRING, 
                                                     CALI_ATTR_SCOPE_THREAD
                                                     | CALI_ATTR_SKIP_EVENTS);
        libpfm_event_name_attr_id = libpfm_event_name_attr.id();


        setup_process_signals();
        
        c->events().create_scope_evt.connect(create_scope_cb);
        c->events().post_init_evt.connect(post_init_cb);
        c->events().finish_evt.connect(finish_cb);
        c->events().pre_flush_evt.connect(pre_flush_cb);
        c->events().postprocess_snapshot.connect(postprocess_snapshot_cb);

        Log(1).stream() << "Registered libpfm service" << endl;
    }

} // namespace


namespace cali 
{
    CaliperService libpfm_service = { "libpfm", ::libpfm_service_register };
} // namespace cali
