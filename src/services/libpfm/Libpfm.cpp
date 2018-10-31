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

// Libpfm.cpp
// libpfm sampling provider for caliper records

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/ContextRecord.h"

#include <mutex>
#include <sstream>
#include <iterator>

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

#include "perf_postprocessing.h"
#include "topdown.h"

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
#define MAX_ATTRIBUTES 12
#define MAX_EVENTS 32

    static cali_id_t libpfm_attributes[MAX_ATTRIBUTES] = {CALI_INV_ID};
    static Attribute libpfm_event_name_attr;
    static cali_id_t libpfm_event_name_attr_id = {CALI_INV_ID};
    static std::vector<cali_id_t> libpfm_event_counter_attr_ids;
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
            {"record_counters", CALI_TYPE_BOOL, "true",
             "Record counter values (true|false)",   
             "Whether to record event counter values at each snapshot (true|false)"
            },
            {"enable_sampling", CALI_TYPE_BOOL, "false",
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
            {"topdown_arch", CALI_TYPE_STRING, "none",
             "Topdown Analysis Architecture",
             "Architecture for which to configure counters and derive metrics for topdown analysis"
            },
            ConfigSet::Terminator
    };

    /*
     * Service configuration variables
     */
    int num_attributes = 0;
    static bool record_counters;
    static bool enable_sampling;
    static std::string events_string;
    static std::string topdown_arch;
    static bool topdown_enabled = false;
    static std::vector<std::string> event_list;
    static std::vector<uint64_t> sampling_period_list;
    static std::vector<uint64_t> precise_ip_list;
    static std::vector<uint64_t> config1_list;

    static std::vector <std::string> sample_attributes_strvec;
    static uint64_t sample_attributes = 0;

    static TopdownObject *topdown_obj = NULL;

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

        uint64_t signals_received;
        uint64_t samples_produced;
        uint64_t bad_samples;
        uint64_t null_events;
        uint64_t null_cali_instances;
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

            if (ehdr.type == PERF_RECORD_SAMPLE) {
                // Read and record
                ret = perf_read_sample(fdx, 1, 0, &ehdr, &sample, stderr); // any way to print to Log(1)?
                if (ret) {
                    Log(1).stream() << "libpfm: cannot read sample" << std::endl;
                }

                // Run handler to push snapshot
                sample_handler(i);
            } else {
                thread_states[thread_id].bad_samples++;
                perf_skip_buffer(fdx, ehdr.size);
            }
        } else {
            thread_states[thread_id].null_events++;
        }

        // Reset
        ret = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        if (ret) {
            Log(0).stream() << "libpfm: unable to re-enable sampling!" << std::endl;
        }
    }

    static void setup_process_events(Caliper *c) {
        int check_num_events = 0;
        perf_event_desc_t *check_fds = NULL;
        int ret = perf_setup_list_events(events_string.c_str(), &check_fds, &check_num_events);

        if (ret || !check_num_events)
            Log(0).stream() << "libpfm: WARNING: invalid event(s) specified!" << std::endl;
        
        if (num_events > MAX_EVENTS)
            Log(0).stream() << "libpfm: WARNING: too many events specified for libpfm service! Maximum is " << MAX_EVENTS << std::endl;

        for(int i=0; i < check_num_events; i++) {
            if (enable_sampling) {
                // Store Caliper nodes for each event name
                event_name_nodes.push_back(
                        c->make_tree_entry(libpfm_event_name_attr,
                                           Variant(CALI_TYPE_STRING, check_fds[i].name, strlen(check_fds[i].name))));
            }
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
        perf_setup_list_events(events_string.c_str(), &fds, &num_events);

        for(i=0; i < num_events; i++) {

            // Set up perf_event
            fds[i].hw.disabled = 1;
            fds[i].hw.read_format = record_counters ? PERF_FORMAT_SCALE : 0;

            if (enable_sampling) {
                fds[i].hw.wakeup_events = 1;
                fds[i].hw.sample_type = sample_attributes;
                fds[i].hw.sample_period = sampling_period_list[i]; 
                fds[i].hw.precise_ip = precise_ip_list[i];
                fds[i].hw.config1 = config1_list[i];
            }

            fds[i].fd = fd = perf_event_open(&fds[i].hw, ts->tid, -1, -1, 0);
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

            fds[i].buf = mmap(NULL, (buffer_pages + 1) * pgsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (fds[i].buf == MAP_FAILED)
                Log(0).stream() << "libpfm: cannot mmap buffer for event " << fds[i].name << std::endl;

            fds[i].pgmsk = (buffer_pages * pgsz) - 1;
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
        int i, ret;
        
        for (i=0; i<num_events; i++) {
            ret = ioctl(fds[i].fd, PERF_EVENT_IOC_RESET, 0);

            if (ret == -1)
                Log(0).stream() << "libpfm: cannot reset counter for event " << fds[i].name << std::endl;

            ret = ioctl(fds[i].fd, PERF_EVENT_IOC_ENABLE, 0);

            if (ret == -1)
                Log(0).stream() << "libpfm: cannot enable event " << fds[i].name << std::endl;
        }

        return ret;
    }

    static int end_thread_sampling() {
        int i, ret;
        size_t pgsz = sysconf(_SC_PAGESIZE);

        for (i=0; i<num_events; i++) {
            ret = ioctl(fds[i].fd, PERF_EVENT_IOC_DISABLE, 0);

            if (ret)
                Log(0).stream() <<  "libpfm: cannot disable event " << fds[i].name << std::endl;

            munmap(fds[i].buf, pgsz);
            close(fds[i].fd);
        }

        perf_free_fds(fds, num_events);

        pfm_terminate();

        return ret;
    }

    static bool parse_configset(Caliper *c) {
        config = RuntimeConfig::init("libpfm", s_configdata);

        enable_sampling = config.get("enable_sampling").to_bool();
        record_counters = config.get("record_counters").to_bool();

        events_string = config.get("events").to_string();
        event_list    = StringConverter(events_string).to_stringlist();


        topdown_arch = config.get("topdown_arch").to_string();
        if (topdown_arch.compare("none") != 0) {
            topdown_obj = new TopdownObject(topdown_arch);
            event_list.insert(event_list.end(), topdown_obj->event_list.begin(), topdown_obj->event_list.end());
        }

        std::ostringstream events_string_oss;
        std::copy(event_list.begin(), event_list.end()-1, std::ostream_iterator<string>(events_string_oss, ",")); 
        events_string_oss << *event_list.rbegin();
        events_string = events_string_oss.str();

        size_t events_listed = event_list.size();

        std::vector<std::string> sampling_period_strvec;
        std::vector<std::string> precise_ip_strvec;
        std::vector<std::string> config1_strvec;

        num_attributes = 0;
        sample_attributes_strvec.clear();

        if (enable_sampling) {
            sample_attributes_strvec = config.get("sample_attributes").to_stringlist();

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

            sampling_period_strvec = config.get("sample_period").to_stringlist();
            precise_ip_strvec      = config.get("precise_ip").to_stringlist();
            config1_strvec         = config.get("config1").to_stringlist();

            if (events_listed != sampling_period_strvec.size()
                | events_listed != precise_ip_strvec.size()
                | events_listed != config1_strvec.size()) {
                
                Log(0).stream() << "libpfm: invalid arguments specified!" << std::endl;
                Log(0).stream() << "libpfm: if sampling enabled, event list, sampling period, precise IP, "
                                << "and config1 must all have the same number of values." << std::endl;

                return false;
            }
        }

        Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
        Variant   v_true(true);

        for (int i=0; i<events_listed; i++) {
            if (enable_sampling) {
                try {
                    sampling_period_list.push_back(std::stoull(sampling_period_strvec[i]));
                    precise_ip_list.push_back(std::stoull(precise_ip_strvec[i]));
                    config1_list.push_back(std::stoull(config1_strvec[i]));
                } catch (...) {
                    Log(0).stream() << "libpfm: invalid arguments specified to libpfm service!" << std::endl;
                    Log(0).stream() << "libpfm: if sampling enabled, sampling period, precise IP, and config1 "
                                    << "must be unsigned integers (uint64_t)" << std::endl;
                    return false;
                }
            }

            // Create attribute for each event counter
            if (record_counters) {
                Attribute event_counter_attr = 
                    c->create_attribute(std::string("libpfm.counter.") + event_list[i],
                                            CALI_TYPE_UINT, 
                                            CALI_ATTR_ASVALUE
                                            | CALI_ATTR_SCOPE_THREAD
                                            | CALI_ATTR_SKIP_EVENTS,
                                            1, &aggr_class_attr, &v_true);
                libpfm_event_counter_attr_ids.push_back(event_counter_attr.id());
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

    struct read_format {
        uint64_t value;     /* The value of the event */
        uint64_t time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
        uint64_t time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
        //uint64_t id;        /* if PERF_FORMAT_ID */
    };

    void snapshot_cb(Caliper* c, int scope, const SnapshotRecord*, SnapshotRecord* snapshot) {
        int i, ret;
        Variant data[MAX_EVENTS];

        struct read_format counter_reads[MAX_EVENTS];

        for (i=0; i<num_events; i++) {

            ret = read(fds[i].fd, &counter_reads[i], sizeof(struct read_format));

            if (ret < sizeof(struct read_format))
                Log(1).stream() << "libpfm: failed to read counter for event " << fds[i].name << std::endl;

            ret = ioctl(fds[i].fd, PERF_EVENT_IOC_RESET, 0);

            if (ret)
                Log(1).stream() << "libpfm: failed to reset counter for event " << fds[i].name << std::endl;
        }

        for (i=0; i<num_events; i++) {
            // FIXME: everyone does this but it does not make sense...
            //      : 1. this calculation scales down rather than interpolate for missing values
            //      : 2. time_enabled and time_running are not reset when counters are reset
            // double raw     = (double)counter_reads[i].value;
            // double enabled = (double)counter_reads[i].time_enabled;
            // double running = (double)counter_reads[i].time_running;
            // uint64_t scaled = (uint64_t)((enabled < running) ? (raw * enabled / running) : raw);
            data[i] = Variant(counter_reads[i].value);
        }

        snapshot->append(num_events, libpfm_event_counter_attr_ids.data(), data);
    }

    void post_init_cb(Caliper* c) {
        // Run on master thread initialization
        setup_process_events(c);
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

        if (enable_sampling) {
            Log(1).stream() << "libpfm: thread sampling stats:" << std::endl;
            for (int i=0; i<num_threads; i++) {
                Log(1).stream() << "libpfm: thread " << i
                                << "\tsignals received: " << thread_states[i].signals_received
                                << "\tsamples produced: " << thread_states[i].samples_produced
                                << "\tbad samples: " << thread_states[i].bad_samples
                                << "\tunknown events: " << thread_states[i].null_events
                                << "\tnull Caliper instances: " << thread_states[i].null_cali_instances << std::endl;
            }
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

        if (topdown_obj != NULL)
            topdown_obj->addDerivedMetricsToSnapshot(c, snapshot);
    }

    static void pre_flush_cb(Caliper* c, const SnapshotRecord* snapshot) {
        if (topdown_obj != NULL) {
            topdown_obj->createTopdownEventAttrMap(c->get_attribute_map());
            topdown_obj->createTopdownMetricAttrMap(c);
        }
    }

    // Initialization handler
    void libpfm_service_register(Caliper* c) {

        if (!parse_configset(c)) {
            Log(0).stream() << "Failed to register libpfm service!" << std::endl;
            return;
        }

        config = RuntimeConfig::init("libpfm", s_configdata);

        libpfm_event_name_attr = 
            c->create_attribute("libpfm.event_sample_name",
                                CALI_TYPE_STRING, 
                                CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);

        libpfm_event_name_attr_id = libpfm_event_name_attr.id();

        if (sample_attributes & PERF_SAMPLE_DATA_SRC) {
            data_src_attrs.mem_lvl_attr   = 
                c->create_attribute("libpfm.memory_level",
                                    CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
            data_src_attrs.mem_hit_attr   = 
                c->create_attribute("libpfm.hit_type",
                                    CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
            data_src_attrs.mem_op_attr    = 
                c->create_attribute("libpfm.operation",
                                    CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
            data_src_attrs.mem_snoop_attr = 
                c->create_attribute("libpfm.snoop",
                                    CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
            data_src_attrs.mem_tlb_attr   = 
                c->create_attribute("libpfm.tlb",
                                    CALI_TYPE_STRING, CALI_ATTR_DEFAULT);
        }

        setup_process_signals();
        
        c->events().create_scope_evt.connect(create_scope_cb);
        c->events().post_init_evt.connect(post_init_cb);
        c->events().finish_evt.connect(finish_cb);

        if (topdown_obj != NULL || enable_sampling)
            c->events().postprocess_snapshot.connect(postprocess_snapshot_cb);

        if (topdown_obj != NULL)
            c->events().pre_flush_evt.connect(pre_flush_cb);

        if (record_counters)
            c->events().snapshot.connect(snapshot_cb);

        Log(1).stream() << "Registered libpfm service" << endl;
    }

} // namespace


namespace cali 
{
    CaliperService libpfm_service = { "libpfm", ::libpfm_service_register };
} // namespace cali
