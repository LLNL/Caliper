// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Libpfm.cpp
// libpfm sampling provider for caliper records

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "../../caliper/MemoryPool.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"

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

#define MAX_ATTRIBUTES 12
#define MAX_EVENTS 32

namespace {

class LibpfmService
{
    cali_id_t libpfm_attributes[MAX_ATTRIBUTES] = {CALI_INV_ID};
    Attribute libpfm_event_name_attr;
    cali_id_t libpfm_event_name_attr_id = {CALI_INV_ID};
    std::vector<cali_id_t> libpfm_event_counter_attr_ids;
    size_t libpfm_attribute_types[MAX_ATTRIBUTES];
    std::map<size_t, Attribute> libpfm_attribute_type_to_attr;
    uint64_t perf_event_sample_t::* sample_attribute_pointers[MAX_ATTRIBUTES];

    std::vector<Node*> event_name_nodes;

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

    static const ConfigSet::Entry s_configdata[];

    /*
     * Service configuration variables
     */

    int num_attributes = 0;
    bool record_counters;
    bool enable_sampling;
    std::string events_string;
    std::vector<std::string> event_list;
    std::vector<uint64_t> sampling_period_list;
    std::vector<uint64_t> precise_ip_list;
    std::vector<uint64_t> config1_list;

    std::vector <std::string> sample_attributes_strvec;
    uint64_t sample_attributes = 0;

    uint64_t signals_received = 0;
    uint64_t samples_produced = 0;
    uint64_t bad_samples  = 0;
    uint64_t null_events  = 0;
    uint64_t null_cali_instances = 0;
    unsigned event_read_fail  = 0;
    unsigned event_reset_fail = 0;

    /*
     * libpfm sampling variables
     */

    struct ThreadState {
        ThreadState()
            : tid(0), fds(nullptr), num_events(0)
            { }

        ~ThreadState()
            {
                if (sI)
                    sI->end_thread_sampling();
            }

        pid_t               tid;
        perf_event_desc_t   *fds;
        int                 num_events;
    };

    static thread_local ThreadState sT;

    static Channel*    sC;
    static LibpfmService* sI;

    const int signum       = SIGIO;
    const int buffer_pages = 1;

    static pid_t gettid(void) {
        return (pid_t) syscall(__NR_gettid);
    }

    inline void sample_handler(int event_index, const perf_event_sample_t& sample) {
        Caliper c = Caliper::sigsafe_instance();

        if (!c) {
            sI->null_cali_instances++;
            return;
        }

        Variant data[MAX_ATTRIBUTES];

        for (int i = 0; i < num_attributes; ++i)
            data[i] = cali_make_variant_from_uint(sample.*sample_attribute_pointers[i]);

        SnapshotRecord info(1, &event_name_nodes[event_index], num_attributes, libpfm_attributes, data);

        c.push_snapshot(sC, &info);

        sI->samples_produced++;
    }

    static void sigio_handler(int sig, siginfo_t *info, void *extra) {
        perf_event_desc_t *fdx = 0;
        struct perf_event_header ehdr;

        int fd = info->si_fd;

        int ret = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        if (ret)
            Log(0).stream() << "libpfm: cannot stop sampling for handling" << std::endl;

        sI->signals_received++;

        int i = 0;
        for (i = 0; i<sT.num_events; i++) {
            if (fd == sT.fds[i].fd) {
                fdx = &sT.fds[i];
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
                perf_event_sample_t sample;
                ret = perf_read_sample(fdx, 1, 0, &ehdr, &sample, stderr); // any way to print to Log(1)?
                if (ret) {
                    Log(1).stream() << "libpfm: cannot read sample" << std::endl;
                }

                // Run handler to push snapshot
                sI->sample_handler(i, sample);
            } else {
                sI->bad_samples++;
                perf_skip_buffer(fdx, ehdr.size);
            }
        } else {
            sI->null_events++;
        }

        // Reset
        ret = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        if (ret) {
            Log(0).stream() << "libpfm: unable to re-enable sampling!" << std::endl;
        }
    }

    void setup_process_events(Caliper *c) {
        int check_num_events = 0;
        perf_event_desc_t *check_fds = NULL;
        int ret = perf_setup_list_events(events_string.c_str(), &check_fds, &check_num_events);

        if (ret || !check_num_events)
            Log(0).stream() << "libpfm: WARNING: invalid event(s) specified!" << std::endl;

        if (check_num_events > MAX_EVENTS)
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

    void setup_thread_events(Caliper *c) {
        struct f_owner_ex fown_ex;
        int ret, fd, flags, i;
        size_t pgsz = sysconf(_SC_PAGESIZE);

        // Set thread state
        sT.tid = gettid();

        // Get perf_event from string
        sT.fds = NULL;
        sT.num_events = 0;

        perf_setup_list_events(events_string.c_str(), &sT.fds, &sT.num_events);

        for(i=0; i < sT.num_events; i++) {

            // Set up perf_event
            sT.fds[i].hw.disabled = 1;
            sT.fds[i].hw.read_format = record_counters ? PERF_FORMAT_SCALE : 0;

            if (enable_sampling) {
                sT.fds[i].hw.wakeup_events = 1;
                sT.fds[i].hw.sample_type = sample_attributes;
                sT.fds[i].hw.sample_period = sampling_period_list[i];
                sT.fds[i].hw.precise_ip = precise_ip_list[i];
                sT.fds[i].hw.config1 = config1_list[i];
            }

            sT.fds[i].fd = fd = perf_event_open(&sT.fds[i].hw, sT.tid, -1, -1, 0);
            if (fd == -1)
                Log(0).stream() << "libpfm: cannot attach event " << sT.fds[i].name << std::endl;

            // Set up perf_event file descriptor to signal this thread
            flags = fcntl(fd, F_GETFL, 0);
            if (fcntl(fd, F_SETFL, flags | O_ASYNC) < 0)
                Log(0).stream() << "libpfm: fcntl SETFL failed" << std::endl;

            fown_ex.type = F_OWNER_TID;
            fown_ex.pid = sT.tid;
            ret = fcntl(fd, F_SETOWN_EX, (unsigned long) &fown_ex);
            if (ret)
                Log(0).stream() << "libpfm: fcntl SETOWN failed" << std::endl;

            if (fcntl(fd, F_SETSIG, signum) < 0)
                Log(0).stream() << "libpfm: fcntl SETSIG failed" << std::endl;

            sT.fds[i].buf = mmap(NULL, (buffer_pages + 1) * pgsz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (sT.fds[i].buf == MAP_FAILED)
                Log(0).stream() << "libpfm: cannot mmap buffer for event " << sT.fds[i].name << std::endl;

            sT.fds[i].pgmsk = (buffer_pages * pgsz) - 1;
        }
    }

    int setup_process_signals() {
        struct sigaction sa;
        sigset_t set, oldsig, newsig;

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

        int ret = sigprocmask(SIG_SETMASK, NULL, &oldsig);

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

    int begin_thread_sampling() {
        int ret = 0;

        for (int i=0; i<sT.num_events; i++) {
            ret = ioctl(sT.fds[i].fd, PERF_EVENT_IOC_RESET, 0);

            if (ret == -1)
                Log(0).stream() << "libpfm: cannot reset counter for event " << sT.fds[i].name << std::endl;

            ret = ioctl(sT.fds[i].fd, PERF_EVENT_IOC_ENABLE, 0);

            if (ret == -1)
                Log(0).stream() << "libpfm: cannot enable event " << sT.fds[i].name << std::endl;
        }

        return ret;
    }

    int end_thread_sampling() {
        int ret = 0;
        size_t pgsz = sysconf(_SC_PAGESIZE);

        for (int i=0; i<sT.num_events; i++) {
            ret = ioctl(sT.fds[i].fd, PERF_EVENT_IOC_DISABLE, 0);

            if (ret)
                Log(0).stream() <<  "libpfm: cannot disable event " << sT.fds[i].name << std::endl;

            munmap(sT.fds[i].buf, pgsz);
            close(sT.fds[i].fd);
        }

        perf_free_fds(sT.fds, sT.num_events);
        pfm_terminate();

        sT.num_events = 0;
        sT.fds = nullptr;

        return ret;
    }

    bool parse_configset(Caliper *c, Channel* chn) {
        ConfigSet config = chn->config().init("libpfm", s_configdata);

        enable_sampling = config.get("enable_sampling").to_bool();
        record_counters = config.get("record_counters").to_bool();

        events_string = config.get("events").to_string();
        event_list    = StringConverter(events_string).to_stringlist();

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
                || events_listed != precise_ip_strvec.size()
                || events_listed != config1_strvec.size()) {

                Log(0).stream() << "libpfm: invalid arguments specified!" << std::endl;
                Log(0).stream() << "libpfm: if sampling enabled, event list, sampling period, precise IP, "
                                << "and config1 must all have the same number of values." << std::endl;

                return false;
            }
        }

        Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
        Variant   v_true(true);

        for (size_t i=0; i<events_listed; i++) {
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

    void setup_sample_pointers() {
        for (int a = 0; a < num_attributes; a++) {
            size_t attribute_type = libpfm_attribute_types[a];

            switch (attribute_type) {
                case (PERF_SAMPLE_IP):
                    sample_attribute_pointers[a] = &perf_event_sample_t::ip;
                    break;
                case (PERF_SAMPLE_ID):
                    sample_attribute_pointers[a] = &perf_event_sample_t::id;
                    break;
                case (PERF_SAMPLE_STREAM_ID):
                    sample_attribute_pointers[a] = &perf_event_sample_t::stream_id;
                    break;
                case (PERF_SAMPLE_TIME):
                    sample_attribute_pointers[a] = &perf_event_sample_t::time;
                    break;
                case (PERF_SAMPLE_TID):
                    sample_attribute_pointers[a] = &perf_event_sample_t::tid;
                    break;
                case (PERF_SAMPLE_PERIOD):
                    sample_attribute_pointers[a] = &perf_event_sample_t::period;
                    break;
                case (PERF_SAMPLE_CPU):
                    sample_attribute_pointers[a] = &perf_event_sample_t::cpu;
                    break;
                case (PERF_SAMPLE_ADDR):
                    sample_attribute_pointers[a] = &perf_event_sample_t::addr;
                    break;
                case (PERF_SAMPLE_WEIGHT):
                    sample_attribute_pointers[a] = &perf_event_sample_t::weight;
                    break;
                case (PERF_SAMPLE_TRANSACTION):
                    sample_attribute_pointers[a] = &perf_event_sample_t::transaction;
                    break;
                case (PERF_SAMPLE_DATA_SRC):
                    sample_attribute_pointers[a] = &perf_event_sample_t::data_src;
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

    void snapshot_cb(Caliper* c, Channel* chn, int scope, const SnapshotRecord*, SnapshotRecord* snapshot) {
        Variant data[MAX_EVENTS] = { Variant() };

        for (int i=0; i<sT.num_events; i++) {
            struct read_format counter_reads = { 0, 0, 0 };

            size_t ret = read(sT.fds[i].fd, &counter_reads, sizeof(struct read_format));

            if (ret < sizeof(struct read_format))
                ++event_read_fail;
            else {
                // FIXME: everyone does this but it does not make sense...
                //      : 1. this calculation scales down rather than interpolate for missing values
                //      : 2. time_enabled and time_running are not reset when counters are reset
                // double raw     = (double)counter_reads[i].value;
                // double enabled = (double)counter_reads[i].time_enabled;
                // double running = (double)counter_reads[i].time_running;
                // uint64_t scaled = (uint64_t)((enabled < running) ? (raw * enabled / running) : raw);
                data[i] = Variant(counter_reads.value);
            }

            int iret = ioctl(sT.fds[i].fd, PERF_EVENT_IOC_RESET, 0);

            if (iret)
                ++event_reset_fail;
        }

        snapshot->append(sT.num_events, libpfm_event_counter_attr_ids.data(), data);
    }

    void post_init_cb(Caliper* c, Channel*) {
        setup_sample_pointers();

        // Run on master thread initialization
        setup_process_events(c);
        setup_thread_events(c);
        begin_thread_sampling();
    }

    void create_thread_cb(Caliper* c, Channel*) {
        setup_thread_events(c);
        begin_thread_sampling();
    }

    void finish_cb(Caliper* c, Channel* chn) {
        end_thread_sampling();
        pfm_terminate();

        if (enable_sampling)
            Log(1).stream() << chn->name() << ": libpfm: thread sampling stats: "
                            << "\tsignals received: " << sI->signals_received
                            << "\tsamples produced: " << sI->samples_produced
                            << "\tbad samples: " << sI->bad_samples
                            << "\tunknown events: " << sI->null_events
                            << "\tnull Caliper instances: " << sI->null_cali_instances << std::endl;
        if (record_counters && (event_read_fail > 0 || event_reset_fail > 0))
            Log(1).stream() << chn->name() << ": libpfm: "
                            << event_read_fail  << " counter reads failed, "
                            << event_reset_fail << " counter resets failed." << std::endl;
    }

    struct DataSrcAttrs {
        Attribute mem_lvl_attr;
        Attribute mem_hit_attr;
        Attribute mem_op_attr;
        Attribute mem_snoop_attr;
        Attribute mem_tlb_attr;
    };

    struct DataSrcAttrs data_src_attrs;

    void postprocess_snapshot_cb(Caliper* c, Channel* chn, std::vector<Entry>& rec) {
        // Decode data_src encoding
        if (sample_attributes & PERF_SAMPLE_DATA_SRC) {
            cali_id_t sample_src_attr_id = libpfm_attribute_type_to_attr[PERF_SAMPLE_DATA_SRC].id();

            auto it = std::find_if(rec.begin(), rec.end(), [&sample_src_attr_id](const Entry& e){
                    return e.attribute() == sample_src_attr_id;
                });

            if (it != rec.end()) {
                uint64_t data_src = it->value().to_uint();

                std::string mem_lvl = datasource_mem_lvl(data_src);
                std::string hit = datasource_mem_hit(data_src);
                std::string op = datasource_mem_op(data_src);
                std::string snoop = datasource_mem_snoop(data_src);
                std::string tlb = datasource_mem_tlb(data_src);

                Node* node = nullptr;

                node = c->make_tree_entry(data_src_attrs.mem_lvl_attr,
                                          Variant(CALI_TYPE_STRING, mem_lvl.c_str(), mem_lvl.size()),
                                          node);
                node = c->make_tree_entry(data_src_attrs.mem_hit_attr,
                                          Variant(CALI_TYPE_STRING, hit.c_str(), hit.size()),
                                          node);
                node = c->make_tree_entry(data_src_attrs.mem_op_attr,
                                          Variant(CALI_TYPE_STRING, op.c_str(), op.size()),
                                          node);
                node = c->make_tree_entry(data_src_attrs.mem_snoop_attr,
                                          Variant(CALI_TYPE_STRING, snoop.c_str(), snoop.size()),
                                          node);
                node = c->make_tree_entry(data_src_attrs.mem_tlb_attr,
                                          Variant(CALI_TYPE_STRING, tlb.c_str(), tlb.size()),
                                          node);

                rec.push_back(Entry(node));
            }
        }
    }

    LibpfmService(Caliper* c, Channel* chn)
        {
            libpfm_event_name_attr =
                c->create_attribute("libpfm.event_sample_name",
                                    CALI_TYPE_STRING,
                                    CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);

            libpfm_event_name_attr_id = libpfm_event_name_attr.id();

            data_src_attrs.mem_lvl_attr   =
                c->create_attribute("libpfm.memory_level",
                                    CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            data_src_attrs.mem_hit_attr   =
                c->create_attribute("libpfm.hit_type",
                                    CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            data_src_attrs.mem_op_attr    =
                c->create_attribute("libpfm.operation",
                                    CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            data_src_attrs.mem_snoop_attr =
                c->create_attribute("libpfm.snoop",
                                    CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
            data_src_attrs.mem_tlb_attr   =
                c->create_attribute("libpfm.tlb",
                                    CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);
        }

public:

    // Initialization handler
    static void libpfm_service_register(Caliper* c, Channel* chn) {
        if (sC) {
            Log(0).stream() << chn->name() << ": libpfm: Cannot enable libpfm service twice!"
                            << " It is already enabled in channel "
                            << sC->name() << std::endl;

            return;
        }

        sI = new LibpfmService(c, chn);

        if (!sI->parse_configset(c, chn)) {
            Log(0).stream() << chn->name() << ": Failed to register libpfm service!" << std::endl;
            return;
        }

        sC = chn;

        sI->setup_process_signals();

        chn->events().create_thread_evt.connect(
            [](Caliper* c, Channel* chn){
                sI->create_thread_cb(c, chn);
            });
        chn->events().post_init_evt.connect(
            [](Caliper* c, Channel* chn){
                sI->post_init_cb(c, chn);
            });
        chn->events().finish_evt.connect(
            [](Caliper* c, Channel* chn){
                sI->finish_cb(c, chn);
                delete sI;
                sI   = nullptr;
                sC = nullptr;
            });

        if (sI->enable_sampling)
            chn->events().postprocess_snapshot.connect(
                [](Caliper* c, Channel* chn, std::vector<Entry>& rec){
                    sI->postprocess_snapshot_cb(c, chn, rec);
                });
        if (sI->record_counters)
            chn->events().snapshot.connect(
                [](Caliper* c, Channel* chn, int scope, const SnapshotRecord* info, SnapshotRecord* rec){
                    sI->snapshot_cb(c, chn, scope, info, rec);
                });

        Log(1).stream() << chn->name() << ": Registered libpfm service" << endl;
    }
}; // class LibpfmService

thread_local LibpfmService::ThreadState LibpfmService::sT;

Channel*       LibpfmService::sC = nullptr;
LibpfmService* LibpfmService::sI = nullptr;

const ConfigSet::Entry LibpfmService::s_configdata[] = {
    {"events", CALI_TYPE_STRING, "cycles",
     "Event list",
     "Comma-separated list of events to sample"
    },
    {"record_counters", CALI_TYPE_BOOL, "true",
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

} // namespace


namespace cali
{

CaliperService libpfm_service = { "libpfm", ::LibpfmService::libpfm_service_register };

} // namespace cali
