// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.
//
// SPDX-License-Identifier: BSD-3-Clause

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

extern "C" {
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include <unistd.h>

#include <x86intrin.h>
}

#include <chrono>
#include <mutex>
#include <tuple>
#include <vector>

using namespace cali;

namespace
{

int topdown_perf_open(struct perf_event_attr* attr_ptr, pid_t pid, int cpu, int group, int flags)
{
    int fd = syscall(SYS_perf_event_open, attr_ptr, pid, cpu, group, flags);

    if (fd < 0)
        Log(0).perror(errno, "syscall(SYS_perf_event_open)");

    return fd;
}

class PerfTopdownService
{
    struct topdown_record_t {
        uint64_t num        { 0 };
        uint64_t slots      { 0 };
        uint64_t retiring   { 0 };
        uint64_t bad_spec   { 0 };
        uint64_t fe_bound   { 0 };
        uint64_t be_bound   { 0 };
        uint64_t heavy_ops  { 0 };
        uint64_t br_mispred { 0 };
        uint64_t fetch_lat  { 0 };
        uint64_t mem_bound  { 0 };
    };

    struct ThreadInfo {
        int   slots_fd;
        int   retiring_fd;
        void* rdpmc_ptr;

        ThreadInfo* next;
        ThreadInfo* prev;

        void unlink()
        {
            if (next)
                next->prev = prev;
            if (prev)
                prev->next = next;
        }

        ThreadInfo(int sl_fd_, int ret_fd_, void* ptr)
            : slots_fd { sl_fd_ }, retiring_fd { ret_fd_ }, rdpmc_ptr { ptr }, next { nullptr }, prev { nullptr }
        { }

        ~ThreadInfo() {
            close(slots_fd);
            close(retiring_fd);
            munmap(rdpmc_ptr, getpagesize());
        }
    };

    ThreadInfo* m_thread_list { nullptr };
    std::mutex  m_thread_list_lock;

    Attribute m_thread_info_attr;

    Attribute m_slots_attr;
    Attribute m_retiring_perc_attr;
    Attribute m_bad_spec_perc_attr;
    Attribute m_fe_bound_perc_attr;
    Attribute m_be_bound_perc_attr;
    Attribute m_heavy_ops_perc_attr;
    Attribute m_light_ops_perc_attr;
    Attribute m_br_mispred_perc_attr;
    Attribute m_fetch_lat_perc_attr;
    Attribute m_fetch_band_perc_attr;
    Attribute m_core_bound_perc_attr;
    Attribute m_mem_bound_perc_attr;
    Attribute m_machine_clears_perc_attr;

    unsigned m_num_errors;

    static PerfTopdownService* s_instance;

    ThreadInfo* get_thread_info(Caliper* c)
    {
        Entry e = c->get(m_thread_info_attr);

        if (e.empty())
            return nullptr;

        return static_cast<ThreadInfo*>(e.value().get_ptr());
    }

    static inline std::tuple<uint64_t, uint64_t> read_topdown(ThreadInfo* t)
    {
        constexpr uint32_t rdpmc_bitmask_fixed = 1ul << 30;
        constexpr uint32_t rdpmc_bitmask_slots = 3ul;
        constexpr uint32_t rdpmc_bitmask_topdown = 1ul << 29;

        uint64_t slots = _rdpmc(rdpmc_bitmask_fixed | rdpmc_bitmask_slots);
        uint64_t td_raw = _rdpmc(rdpmc_bitmask_topdown);

        ioctl(t->slots_fd, PERF_EVENT_IOC_RESET);

        return std::make_tuple(slots, td_raw);
    }

    void snapshot_cb(Caliper* c, SnapshotView /*trigger_info*/, SnapshotBuilder& rec)
    {
        ThreadInfo* td = get_thread_info(c);

        if (td == nullptr)
            return;
        
        uint64_t slots = 0, td_raw = 0;
        std::tie(slots, td_raw) = read_topdown(td);

        double retiring_perc = static_cast<double>((td_raw >>  0) & 0xffull) / 255.0 * 100.0;
        double bad_spec_perc = static_cast<double>((td_raw >>  8) & 0xffull) / 255.0 * 100.0;
        double fe_bound_perc = static_cast<double>((td_raw >> 16) & 0xffull) / 255.0 * 100.0;
        double be_bound_perc = static_cast<double>((td_raw >> 24) & 0xffull) / 255.0 * 100.0;

        rec.append(m_slots_attr, cali_make_variant_from_uint(slots));
        rec.append(m_retiring_perc_attr, cali_make_variant_from_double(retiring_perc));
        rec.append(m_bad_spec_perc_attr, cali_make_variant_from_double(bad_spec_perc));
        rec.append(m_fe_bound_perc_attr, cali_make_variant_from_double(fe_bound_perc));
        rec.append(m_be_bound_perc_attr, cali_make_variant_from_double(be_bound_perc));
    }

    void init_thread(Caliper* c, Channel* channel)
    {
        struct perf_event_attr slots_attr = {
            .type = PERF_TYPE_RAW,
            .size = sizeof(struct perf_event_attr),
            .config = 0x400,
            .read_format = PERF_FORMAT_GROUP,
            .disabled = 1,
        };

        int slots_fd = topdown_perf_open(&slots_attr, 0, -1, -1, 0);
        if (slots_fd < 0) {
            Log(0).stream() << ": " << channel->name() << ": perf_topdown: cannot open slots fd";
            return;
        }

        struct perf_event_attr retiring_attr = {
            .type = PERF_TYPE_RAW,
            .size = sizeof(struct perf_event_attr),
            .config = 0x8000,
            .read_format = PERF_FORMAT_GROUP,
            .disabled = 0,
        };

        int retiring_fd = topdown_perf_open(&retiring_attr, 0, -1, slots_fd, 0);
        if (retiring_fd < 0) {
            close(slots_fd);
            Log(0).stream() << ": " << channel->name() << ": perf_topdown: cannot open retiring fd";
            return;
        }

        void* rdpmc_ptr = mmap(nullptr, getpagesize(), PROT_READ, MAP_SHARED, retiring_fd, 0);
        if (rdpmc_ptr == MAP_FAILED) {
            close(slots_fd);
            close(retiring_fd);
            Log(0).perror(errno, "mmap") << ": perf_topdown: mmap for rdpmc failed";
            return;
        }

        // --- create thread info

        ThreadInfo* td = new ThreadInfo(slots_fd, retiring_fd, rdpmc_ptr);

        {
            std::lock_guard<std::mutex> g(m_thread_list_lock);

            if (m_thread_list)
                m_thread_list->prev = td;

            td->next      = m_thread_list;
            m_thread_list = td;
        }

        c->set(m_thread_info_attr, cali_make_variant_from_ptr(td));

        // --- reset and start counting

        ioctl(slots_fd, PERF_EVENT_IOC_RESET);
        ioctl(slots_fd, PERF_EVENT_IOC_ENABLE);
    }

    void finish_thread(Caliper* c)
    {
        ThreadInfo* td = get_thread_info(c);

        if (!td)
            return;

        std::lock_guard<std::mutex> g(m_thread_list_lock);

        ThreadInfo* tmp = td->next;
        td->unlink();

        if (td == m_thread_list)
            m_thread_list = tmp;

        delete td;
    }

    void post_init_cb(Caliper* c, Channel* channel)
    {
        init_thread(c, channel);
    }

    void finish_cb(Caliper* c, Channel* channel)
    {
        finish_thread(c);
    }

    PerfTopdownService(Caliper* c, Channel* channel)
        : m_num_errors(0)
    {
        m_thread_info_attr =
            c->create_attribute(std::string("topdown.thread.")+std::to_string(channel->id()),
                CALI_TYPE_PTR,
                CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_HIDDEN | CALI_ATTR_SKIP_EVENTS);

        m_slots_attr = 
            c->create_attribute("topdown.slots", CALI_TYPE_UINT, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_retiring_perc_attr =
            c->create_attribute("topdown.retiring", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_bad_spec_perc_attr =
            c->create_attribute("topdown.bad_spec", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_fe_bound_perc_attr =
            c->create_attribute("topdown.fe_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_be_bound_perc_attr =
            c->create_attribute("topdown.be_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
#if 0
        m_heavy_ops_perc_attr =
            c->create_attribute("topdown.heavy_ops", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_light_ops_perc_attr =
            c->create_attribute("topdown.light_ops", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_br_mispred_perc_attr =
            c->create_attribute("topdown.br_mispred", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_fetch_lat_perc_attr =
            c->create_attribute("topdown.fetch_lat", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_fetch_band_perc_attr =
            c->create_attribute("topdown.fetch_band", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_mem_bound_perc_attr =
            c->create_attribute("topdown.mem_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_core_bound_perc_attr =
            c->create_attribute("topdown.core_bound", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
        m_machine_clears_perc_attr =
            c->create_attribute("topdown.machine_clears", CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE);
#endif
    }

public:

    static const char* s_spec;

    static void register_perf_topdown(Caliper* c, Channel* channel)
    {
        if (s_instance) {
            Log(0).stream() << ": " << channel->name() << ": perf_topdown is already active, disabling!";
            return;
        }

        s_instance = new PerfTopdownService(c, channel);

        channel->events().post_init_evt.connect([](Caliper* c, Channel* channel) {
            s_instance->post_init_cb(c, channel);
        });
        channel->events().create_thread_evt.connect([](Caliper* c, Channel* channel) {
            s_instance->init_thread(c, channel);
        });
        channel->events().release_thread_evt.connect([](Caliper* c, Channel*) {
            s_instance->finish_thread(c);
        });
        channel->events().snapshot.connect(
            [](Caliper* c, SnapshotView trigger_info, SnapshotBuilder& rec) {
                s_instance->snapshot_cb(c, trigger_info, rec);
            }
        );
        channel->events().finish_evt.connect([](Caliper* c, Channel* channel) {
            s_instance->finish_cb(c, channel);
            delete s_instance;
            s_instance = nullptr;
        });

        Log(1).stream() << channel->name() << ": Registered perf_topdown service" << std::endl;
    }
};

PerfTopdownService* PerfTopdownService::s_instance { nullptr };

const char* PerfTopdownService::s_spec = R"json(
{
 "name": "perf_topdown",
 "description": "Intel topdown metrics via perf"
}
)json";

} // namespace

namespace cali
{

CaliperService perf_topdown_service = { ::PerfTopdownService::s_spec, ::PerfTopdownService::register_perf_topdown };

} // namespace cali
