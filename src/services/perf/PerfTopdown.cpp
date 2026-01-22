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

    enum Level { Top, All };

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

    Level m_level;

    Attribute m_thread_info_attr;

    Attribute m_slots_attr;
    Attribute m_slots_sum_attr;

    Attribute m_retiring_slots_attr;
    Attribute m_bad_spec_slots_attr;
    Attribute m_fe_bound_slots_attr;
    Attribute m_be_bound_slots_attr;
    Attribute m_heavy_ops_slots_attr;
    Attribute m_br_mispred_slots_attr;
    Attribute m_fetch_lat_slots_attr;
    Attribute m_mem_bound_slots_attr;

    Attribute m_retiring_sum_attr;
    Attribute m_bad_spec_sum_attr;
    Attribute m_fe_bound_sum_attr;
    Attribute m_be_bound_sum_attr;
    Attribute m_heavy_ops_sum_attr;
    Attribute m_br_mispred_sum_attr;
    Attribute m_fetch_lat_sum_attr;
    Attribute m_mem_bound_sum_attr;

    Attribute m_retiring_perc_attr;
    Attribute m_bad_spec_perc_attr;
    Attribute m_fe_bound_perc_attr;
    Attribute m_be_bound_perc_attr;
    Attribute m_heavy_ops_perc_attr;
    Attribute m_light_ops_perc_attr;
    Attribute m_br_mispred_perc_attr;
    Attribute m_fetch_lat_perc_attr;
    Attribute m_fetch_bw_perc_attr;
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

        if (slots < 480'000ull)
            return std::make_tuple(0ull, 0ull);

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

        if (slots == 0)
            return;

        uint64_t slots_factor = slots / 255ull;

        uint64_t retiring_slots = ((td_raw >>  0) & 0xffull) * slots_factor;
        uint64_t bad_spec_slots = ((td_raw >>  8) & 0xffull) * slots_factor;
        uint64_t fe_bound_slots = ((td_raw >> 16) & 0xffull) * slots_factor;
        uint64_t be_bound_slots = ((td_raw >> 24) & 0xffull) * slots_factor;

        rec.append(m_slots_attr, cali_make_variant_from_uint(slots));

        rec.append(m_retiring_slots_attr, cali_make_variant_from_uint(retiring_slots));
        rec.append(m_bad_spec_slots_attr, cali_make_variant_from_uint(bad_spec_slots));
        rec.append(m_fe_bound_slots_attr, cali_make_variant_from_uint(fe_bound_slots));
        rec.append(m_be_bound_slots_attr, cali_make_variant_from_uint(be_bound_slots));

        if (m_level == Level::Top)
            return;

        uint64_t heavy_ops_slots  = ((td_raw >> 32) & 0xffull) * slots_factor;
        uint64_t br_mispred_slots = ((td_raw >> 40) & 0xffull) * slots_factor;
        uint64_t fetch_lat_slots  = ((td_raw >> 48) & 0xffull) * slots_factor;
        uint64_t mem_bound_slots  = ((td_raw >> 56) & 0xffull) * slots_factor;

        rec.append(m_heavy_ops_slots_attr, cali_make_variant_from_uint(heavy_ops_slots));
        rec.append(m_br_mispred_slots_attr, cali_make_variant_from_uint(br_mispred_slots));
        rec.append(m_fetch_lat_slots_attr, cali_make_variant_from_uint(fetch_lat_slots));
        rec.append(m_mem_bound_slots_attr, cali_make_variant_from_uint(mem_bound_slots));
    }

    void postprocess_snapshot_cb(Caliper* /*c*/, std::vector<Entry>& rec)
    {
        SnapshotView rec_v(rec.size(), rec.data());
        Entry slots_e = rec_v.get_immediate_entry(m_slots_sum_attr);
        if (slots_e.empty())
            slots_e = rec_v.get_immediate_entry(m_slots_attr);
        if (slots_e.empty())
            return;

        double perc_factor = 100.0 / slots_e.value().to_double();

        auto get_value = [perc_factor](const std::vector<Entry>& rec, Attribute a, Attribute b) -> double {
            cali_id_t a_id = a.id();
            cali_id_t b_id = b.id();
            for (const Entry& e : rec) {
                cali_id_t n_id = e.node() ? e.node()->id() : CALI_INV_ID;
                if (n_id == a_id || n_id == b_id)
                    return e.value().to_double() * perc_factor;
            }
            return 0.0;
        };

        double retiring = get_value(rec, m_retiring_sum_attr, m_retiring_slots_attr);
        double bad_spec = get_value(rec, m_bad_spec_sum_attr, m_bad_spec_slots_attr);
        double fe_bound = get_value(rec, m_fe_bound_sum_attr, m_fe_bound_slots_attr);
        double be_bound = get_value(rec, m_be_bound_sum_attr, m_be_bound_slots_attr);

        rec.push_back(Entry(m_retiring_perc_attr, Variant(retiring)));
        rec.push_back(Entry(m_bad_spec_perc_attr, Variant(bad_spec)));
        rec.push_back(Entry(m_fe_bound_perc_attr, Variant(fe_bound)));
        rec.push_back(Entry(m_be_bound_perc_attr, Variant(be_bound)));

        if (m_level == Level::Top)
            return;

        double heavy_ops = get_value(rec, m_heavy_ops_sum_attr, m_heavy_ops_slots_attr);
        double br_mispred = get_value(rec, m_br_mispred_sum_attr, m_br_mispred_slots_attr);
        double fetch_lat = get_value(rec, m_fetch_lat_sum_attr, m_fetch_lat_slots_attr);
        double mem_bound = get_value(rec, m_mem_bound_sum_attr, m_mem_bound_slots_attr);

        rec.push_back(Entry(m_heavy_ops_perc_attr, Variant(heavy_ops)));
        rec.push_back(Entry(m_light_ops_perc_attr, Variant(retiring - heavy_ops)));
        rec.push_back(Entry(m_br_mispred_perc_attr, Variant(br_mispred)));
        rec.push_back(Entry(m_machine_clears_perc_attr, Variant(bad_spec - br_mispred)));
        rec.push_back(Entry(m_fetch_lat_perc_attr, Variant(fetch_lat)));
        rec.push_back(Entry(m_fetch_bw_perc_attr, Variant(fe_bound - fetch_lat)));
        rec.push_back(Entry(m_mem_bound_perc_attr, Variant(mem_bound)));
        rec.push_back(Entry(m_core_bound_perc_attr, Variant(be_bound - mem_bound)));
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
            Log(0).stream() << ": " << channel->name() << ": perf_topdown: cannot open retiring fd";
            close(slots_fd);
            return;
        }

        void* rdpmc_ptr = mmap(nullptr, getpagesize(), PROT_READ, MAP_SHARED, retiring_fd, 0);
        if (rdpmc_ptr == MAP_FAILED) {
            Log(0).perror(errno, "mmap") << ": perf_topdown: mmap for rdpmc failed";
            close(slots_fd);
            close(retiring_fd);
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

        Log(1).stream() << channel->name() << ": perf_topdown active, level=" << (m_level == Level::Top ? "top\n" : "all\n");

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

    void create_attributes(Caliper* c, Channel* channel)
    {
        m_thread_info_attr =
            c->create_attribute(std::string("topdown.thread.")+std::to_string(channel->id()),
                CALI_TYPE_PTR,
                CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_HIDDEN | CALI_ATTR_SKIP_EVENTS);

        constexpr int slots_attr_prop = CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_AGGREGATABLE | CALI_ATTR_SKIP_EVENTS;
        constexpr int sum_attr_prop = CALI_ATTR_SCOPE_THREAD | CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS;
        constexpr int perc_attr_prop = CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS;

        m_slots_attr = c->create_attribute("topdown.slots", CALI_TYPE_UINT, slots_attr_prop);
        m_slots_sum_attr = c->create_attribute("sum#topdown.slots", CALI_TYPE_UINT, sum_attr_prop);

        m_retiring_slots_attr = c->create_attribute("topdown.retiring.slots", CALI_TYPE_UINT, slots_attr_prop);
        m_bad_spec_slots_attr = c->create_attribute("topdown.bad_spec.slots", CALI_TYPE_UINT, slots_attr_prop);
        m_fe_bound_slots_attr = c->create_attribute("topdown.fe_bound.slots", CALI_TYPE_UINT, slots_attr_prop);
        m_be_bound_slots_attr = c->create_attribute("topdown.be_bound.slots", CALI_TYPE_UINT, slots_attr_prop);

        m_retiring_sum_attr = c->create_attribute("sum#topdown.retiring.slots", CALI_TYPE_UINT, sum_attr_prop);
        m_bad_spec_sum_attr = c->create_attribute("sum#topdown.bad_spec.slots", CALI_TYPE_UINT, sum_attr_prop);
        m_fe_bound_sum_attr = c->create_attribute("sum#topdown.fe_bound.slots", CALI_TYPE_UINT, sum_attr_prop);
        m_be_bound_sum_attr = c->create_attribute("sum#topdown.be_bound.slots", CALI_TYPE_UINT, sum_attr_prop);

        m_retiring_perc_attr = c->create_attribute("topdown.retiring", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_bad_spec_perc_attr = c->create_attribute("topdown.bad_spec", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_fe_bound_perc_attr = c->create_attribute("topdown.fe_bound", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_be_bound_perc_attr = c->create_attribute("topdown.be_bound", CALI_TYPE_DOUBLE, perc_attr_prop);

        if (m_level == Level::Top)
            return;

        m_heavy_ops_slots_attr = c->create_attribute("topdown.heavy_ops.slots", CALI_TYPE_UINT, slots_attr_prop);
        m_br_mispred_slots_attr = c->create_attribute("topdown.br_mispred.slots", CALI_TYPE_UINT, slots_attr_prop);
        m_fetch_lat_slots_attr = c->create_attribute("topdown.fetch_lat.slots", CALI_TYPE_UINT, slots_attr_prop);
        m_mem_bound_slots_attr = c->create_attribute("topdown.mem_bound.slots", CALI_TYPE_UINT, slots_attr_prop);

        m_heavy_ops_sum_attr = c->create_attribute("sum#topdown.heavy_ops.slots", CALI_TYPE_UINT, sum_attr_prop);
        m_br_mispred_sum_attr = c->create_attribute("sum#topdown.br_mispred.slots", CALI_TYPE_UINT, sum_attr_prop);
        m_fetch_lat_sum_attr = c->create_attribute("sum#topdown.fetch_lat.slots", CALI_TYPE_UINT, sum_attr_prop);
        m_mem_bound_sum_attr = c->create_attribute("sum#topdown.mem_bound.slots", CALI_TYPE_UINT, sum_attr_prop);

        m_heavy_ops_perc_attr = c->create_attribute("topdown.heavy_ops", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_light_ops_perc_attr = c->create_attribute("topdown.light_ops", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_br_mispred_perc_attr = c->create_attribute("topdown.br_mispred", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_machine_clears_perc_attr = c->create_attribute("topdown.machine_clears", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_fetch_lat_perc_attr = c->create_attribute("topdown.fetch_lat", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_fetch_bw_perc_attr = c->create_attribute("topdown.fetch_bw", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_mem_bound_perc_attr = c->create_attribute("topdown.mem_bound", CALI_TYPE_DOUBLE, perc_attr_prop);
        m_core_bound_perc_attr = c->create_attribute("topdown.core_bound", CALI_TYPE_DOUBLE, perc_attr_prop);
    }

    PerfTopdownService(Caliper* c, Channel* channel)
        : m_level { Level::Top }
        , m_num_errors { 0 }
    {
        auto cfg = services::init_config_from_spec(channel->config(), s_spec);

        std::string level_str = cfg.get("level").to_string();
        if (level_str == "all")
            m_level = Level::All;
        else if (level_str != "top")
            Log(0).stream() << channel->name() << ": perf_topdown: invalid value \"" << level_str << "\" for level option";

        create_attributes(c, channel);
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
        channel->events().postprocess_snapshot.connect(
            [](Caliper* c, std::vector<Entry>& rec){
                s_instance->postprocess_snapshot_cb(c, rec);
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
 "description": "Intel topdown metrics via perf",
 "config":
 [
  { "name": "level",
    "description": "Topdown level ('top' or 'all')",
    "type": "string",
    "value": "top"
  }
 ]
}
)json";

} // namespace

namespace cali
{

CaliperService perf_topdown_service = { ::PerfTopdownService::s_spec, ::PerfTopdownService::register_perf_topdown };

} // namespace cali
