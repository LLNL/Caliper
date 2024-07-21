// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// IntelTopdown.cpp
// Intel top-down analysis

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/StringConverter.h"

#include "../Services.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <vector>

using namespace cali;

namespace
{

enum IntelTopdownLevel { All = 1, Top = 2 };

class TopdownCalculator
{
protected:

    IntelTopdownLevel m_level;

    const char* m_top_counters;
    const char* m_all_counters;

    std::vector<const char*> m_res_top;
    std::vector<const char*> m_res_all;

    std::map<std::string, Attribute> m_counter_attrs;
    std::map<std::string, Attribute> m_result_attrs;

    std::map<std::string, int> m_counters_not_found;

    Variant get_val_from_rec(const std::vector<Entry>& rec, const char* name)
    {
        Variant ret;

        auto c_it = m_counter_attrs.find(name);
        if (c_it == m_counter_attrs.end())
            return ret;

        cali_id_t attr_id = c_it->second.id();

        auto it = std::find_if(rec.begin(), rec.end(), [attr_id](const Entry& e) { return e.attribute() == attr_id; });

        if (it != rec.end())
            ret = it->value();
        else
            ++m_counters_not_found[std::string(name)];

        return ret;
    }

    TopdownCalculator(
        IntelTopdownLevel          level,
        const char*                top_counters,
        const char*                all_counters,
        std::vector<const char*>&& res_top,
        std::vector<const char*>&& res_all
    )
        : m_level(level),
          m_top_counters(top_counters),
          m_all_counters(all_counters),
          m_res_top(res_top),
          m_res_all(res_all)
    {}

public:

    TopdownCalculator(IntelTopdownLevel level) : m_level(level) {}

    virtual ~TopdownCalculator() = default;

    virtual std::vector<Entry> compute_toplevel(const std::vector<Entry>& rec) = 0;

    virtual std::size_t get_num_expected_toplevel() const = 0;

    virtual std::vector<Entry> compute_retiring(const std::vector<Entry>& rec) = 0;

    virtual std::size_t get_num_expected_retiring() const = 0;

    virtual std::vector<Entry> compute_backend_bound(const std::vector<Entry>& rec) = 0;

    virtual std::size_t get_num_expected_backend_bound() const = 0;

    virtual std::vector<Entry> compute_frontend_bound(const std::vector<Entry>& rec) = 0;

    virtual std::size_t get_num_expected_frontend_bound() const = 0;

    virtual std::vector<Entry> compute_bad_speculation(const std::vector<Entry>& rec) = 0;

    virtual std::size_t get_num_expected_bad_speculation() const = 0;

    bool find_counter_attrs(CaliperMetadataAccessInterface& db)
    {
        const char* list     = (m_level == All ? m_all_counters : m_top_counters);
        auto        counters = StringConverter(list).to_stringlist();

        for (const auto& s : counters) {
            Attribute attr = db.get_attribute(std::string("sum#papi.") + s);

            if (!attr)
                attr = db.get_attribute(std::string("papi.") + s);
            if (!attr) {
                Log(0).stream() << "topdown: " << s << " counter attribute not found!" << std::endl;
                return false;
            }

            m_counter_attrs[s] = attr;
        }

        return true;
    }

    void make_result_attrs(CaliperMetadataAccessInterface& db)
    {
        std::vector<const char*>& res = (m_level == Top ? m_res_top : m_res_all);

        for (const char* s : res) {
            m_result_attrs[std::string(s)] = db.create_attribute(
                std::string("topdown.") + s,
                CALI_TYPE_DOUBLE,
                CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS
            );
        }
    }

    const std::map<std::string, int>& get_counters_not_found() const { return m_counters_not_found; }

    const char* get_counters() const
    {
        if (m_level == All) {
            return m_all_counters;
        } else {
            return m_top_counters;
        }
    }

    IntelTopdownLevel get_level() const { return m_level; }
};

class HaswellTopdown : public TopdownCalculator
{
public:

    HaswellTopdown(IntelTopdownLevel level)
        : TopdownCalculator(
            level,
            // top_counters
            "CPU_CLK_THREAD_UNHALTED:THREAD_P"
            ",IDQ_UOPS_NOT_DELIVERED:CORE"
            ",INT_MISC:RECOVERY_CYCLES"
            ",UOPS_ISSUED:ANY"
            ",UOPS_RETIRED:RETIRE_SLOTS",
            // all_counters
            "BR_MISP_RETIRED:ALL_BRANCHES"
            ",CPU_CLK_THREAD_UNHALTED:THREAD_P"
            ",CYCLE_ACTIVITY:CYCLES_NO_EXECUTE"
            ",CYCLE_ACTIVITY:STALLS_L1D_PENDING"
            ",CYCLE_ACTIVITY:STALLS_L2_PENDING"
            ",CYCLE_ACTIVITY:STALLS_LDM_PENDING"
            ",IDQ_UOPS_NOT_DELIVERED:CORE"
            ",IDQ_UOPS_NOT_DELIVERED:CYCLES_0_UOPS_DELIV_CORE"
            ",INT_MISC:RECOVERY_CYCLES"
            ",MACHINE_CLEARS:COUNT"
            ",MEM_LOAD_UOPS_RETIRED:L3_HIT"
            ",MEM_LOAD_UOPS_RETIRED:L3_MISS"
            ",UOPS_EXECUTED:CORE_CYCLES_GE_1"
            ",UOPS_EXECUTED:CORE_CYCLES_GE_2"
            ",UOPS_ISSUED:ANY"
            ",UOPS_RETIRED:RETIRE_SLOTS",
            // res_top
            { "retiring", "backend_bound", "frontend_bound", "bad_speculation" },
            // res_all
            { "retiring",
              "backend_bound",
              "frontend_bound",
              "bad_speculation",
              "branch_mispredict",
              "machine_clears",
              "frontend_latency",
              "frontend_bandwidth",
              "memory_bound",
              "core_bound",
              "ext_mem_bound",
              "l1_bound",
              "l2_bound",
              "l3_bound" }
        )
    {}

    virtual ~HaswellTopdown() = default;

    virtual std::vector<Entry> compute_toplevel(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        Variant v_cpu_clk_unhalted_thread_p   = get_val_from_rec(rec, "CPU_CLK_THREAD_UNHALTED:THREAD_P");
        Variant v_uops_retired_retire_slots   = get_val_from_rec(rec, "UOPS_RETIRED:RETIRE_SLOTS");
        Variant v_uops_issued_any             = get_val_from_rec(rec, "UOPS_ISSUED:ANY");
        Variant v_int_misc_recovery_cycles    = get_val_from_rec(rec, "INT_MISC:RECOVERY_CYCLES");
        Variant v_idq_uops_not_delivered_core = get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CORE");

        bool is_incomplete = v_cpu_clk_unhalted_thread_p.empty() || v_uops_retired_retire_slots.empty()
                             || v_uops_issued_any.empty() || v_int_misc_recovery_cycles.empty()
                             || v_idq_uops_not_delivered_core.empty();
        bool is_nonzero = v_cpu_clk_unhalted_thread_p.to_double() > 0.0 && v_uops_retired_retire_slots.to_double() > 0.0
                          && v_uops_issued_any.to_double() > 0.0 && v_int_misc_recovery_cycles.to_double() > 0.0
                          && v_idq_uops_not_delivered_core.to_double() > 0.0;

        double slots = 4.0 * v_cpu_clk_unhalted_thread_p.to_double();

        if (is_incomplete || !is_nonzero || slots < 1.0)
            return ret;

        double retiring        = v_uops_retired_retire_slots.to_double() / slots;
        double bad_speculation = (v_uops_issued_any.to_double() - v_uops_retired_retire_slots.to_double()
                                  + 4.0 * v_int_misc_recovery_cycles.to_double())
                                 / slots;
        double frontend_bound = v_idq_uops_not_delivered_core.to_double() / slots;
        double backend_bound  = 1.0 - (retiring + bad_speculation + frontend_bound);

        ret.reserve(4);
        ret.push_back(Entry(m_result_attrs["retiring"], Variant(std::max(retiring, 0.0))));
        ret.push_back(Entry(m_result_attrs["backend_bound"], Variant(std::max(backend_bound, 0.0))));
        ret.push_back(Entry(m_result_attrs["frontend_bound"], Variant(std::max(frontend_bound, 0.0))));
        ret.push_back(Entry(m_result_attrs["bad_speculation"], Variant(std::max(bad_speculation, 0.0))));

        return ret;
    }

    virtual std::size_t get_num_expected_toplevel() const override { return 4; }

    virtual std::vector<Entry> compute_retiring(const std::vector<Entry>& rec) override { return {}; }

    virtual std::size_t get_num_expected_retiring() const override { return 0; }

    virtual std::vector<Entry> compute_backend_bound(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        Variant v_cpu_clk_unhalted_thread_p         = get_val_from_rec(rec, "CPU_CLK_THREAD_UNHALTED:THREAD_P");
        Variant v_cycle_activity_stalls_ldm_pending = get_val_from_rec(rec, "CYCLE_ACTIVITY:STALLS_LDM_PENDING");
        Variant v_cycle_activity_cycles_no_execute  = get_val_from_rec(rec, "CYCLE_ACTIVITY:CYCLES_NO_EXECUTE");
        Variant v_uops_executed_core_cycles_ge_1    = get_val_from_rec(rec, "UOPS_EXECUTED:CORE_CYCLES_GE_1");
        Variant v_uops_executed_core_cycles_ge_2    = get_val_from_rec(rec, "UOPS_EXECUTED:CORE_CYCLES_GE_2");
        Variant v_mem_load_uops_retired_l3_miss     = get_val_from_rec(rec, "MEM_LOAD_UOPS_RETIRED:L3_MISS");
        Variant v_mem_load_uops_retired_l3_hit      = get_val_from_rec(rec, "MEM_LOAD_UOPS_RETIRED:L3_HIT");
        Variant v_cycle_activity_stalls_l2_pending  = get_val_from_rec(rec, "CYCLE_ACTIVITY:STALLS_L2_PENDING");
        Variant v_cycle_activity_stalls_l1d_pending = get_val_from_rec(rec, "CYCLE_ACTIVITY:STALLS_L1D_PENDING");

        bool is_incomplete = v_cpu_clk_unhalted_thread_p.empty() || v_cycle_activity_stalls_ldm_pending.empty()
                             || v_cycle_activity_cycles_no_execute.empty() || v_uops_executed_core_cycles_ge_1.empty()
                             || v_uops_executed_core_cycles_ge_2.empty() || v_mem_load_uops_retired_l3_miss.empty()
                             || v_mem_load_uops_retired_l3_hit.empty() || v_cycle_activity_stalls_l2_pending.empty()
                             || v_cycle_activity_stalls_l1d_pending.empty();

        double clocks = v_cpu_clk_unhalted_thread_p.to_double();

        if (is_incomplete || !(clocks > 1.0))
            return ret;

        double memory_bound = v_cycle_activity_stalls_ldm_pending.to_double() / clocks;
        double be_bound_at_exe =
            (v_cycle_activity_cycles_no_execute.to_double() + v_uops_executed_core_cycles_ge_1.to_double()
             - v_uops_executed_core_cycles_ge_2.to_double())
            / clocks;
        double l3_tot = v_mem_load_uops_retired_l3_hit.to_double() + 7.0 * v_mem_load_uops_retired_l3_miss.to_double();
        double l3_hit_fraction  = 0.0;
        double l3_miss_fraction = 0.0;
        if (l3_tot > 0.0) {
            l3_hit_fraction  = v_mem_load_uops_retired_l3_hit.to_double() / l3_tot;
            l3_miss_fraction = v_mem_load_uops_retired_l3_miss.to_double() / l3_tot;
        }
        double ext_mem_bound = v_cycle_activity_stalls_l2_pending.to_double() * l3_miss_fraction / clocks;
        double l1_bound =
            (v_cycle_activity_stalls_ldm_pending.to_double() - v_cycle_activity_stalls_l1d_pending.to_double())
            / clocks;
        double l2_bound =
            (v_cycle_activity_stalls_l1d_pending.to_double() - v_cycle_activity_stalls_l2_pending.to_double()) / clocks;
        double l3_bound = v_cycle_activity_stalls_l2_pending.to_double() * l3_hit_fraction / clocks;

        ret.reserve(6);
        ret.push_back(Entry(result_attrs["memory_bound"], Variant(memory_bound)));
        ret.push_back(Entry(result_attrs["core_bound"], Variant(be_bound_at_exe - memory_bound)));
        ret.push_back(Entry(result_attrs["ext_mem_bound"], Variant(ext_mem_bound)));
        ret.push_back(Entry(result_attrs["l1_bound"], Variant(l1_bound)));
        ret.push_back(Entry(result_attrs["l2_bound"], Variant(l2_bound)));
        ret.push_back(Entry(result_attrs["l3_bound"], Variant(l3_bound)));

        return ret;
    }

    virtual std::size_t get_num_expected_backend_bound() const override { return 6; }

    virtual std::vector<Entry> compute_frontend_bound(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        Variant v_cpu_clk_unhalted_thread_p = get_val_from_rec(rec, "CPU_CLK_THREAD_UNHALTED:THREAD_P");
        Variant v_idq_uops_not_delivered    = get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CYCLES_0_UOPS_DELIV_CORE");

        bool is_incomplete = v_cpu_clk_unhalted_thread_p.empty() || v_idq_uops_not_delivered.empty();

        double clocks = v_cpu_clk_unhalted_thread_p.to_double();
        double uops   = v_idq_uops_not_delivered.to_double();

        if (is_incomplete || clocks < 1.0 || uops > clocks)
            return ret;

        double fe_latency = uops / clocks;

        ret.reserve(2);
        ret.push_back(Entry(result_attrs["frontend_latency"], Variant(fe_latency)));
        ret.push_back(Entry(result_attrs["frontend_bandwidth"], Variant(1.0 - fe_latency)));

        return ret;
    }

    virtual std::size_t get_num_expected_frontend_bound() const override { return 2; }

    virtual std::vector<Entry> compute_bad_speculation(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        Variant v_br_misp_retired_all_branches = get_val_from_rec(rec, "BR_MISP_RETIRED:ALL_BRANCHES");
        Variant v_machine_clears_count         = get_val_from_rec(rec, "MACHINE_CLEARS:COUNT");

        bool is_incomplete = v_br_misp_retired_all_branches.empty() || v_machine_clears_count.empty();

        double br_misp_retired_all_branches = v_br_misp_retired_all_branches.to_double();
        double machine_clears_count         = v_machine_clears_count.to_double();

        if (is_incomplete || !(br_misp_retired_all_branches + machine_clears_count > 1.0))
            return ret;

        double branch_mispredict = br_misp_retired_all_branches / (br_misp_retired_all_branches + machine_clears_count);

        ret.reserve(2);
        ret.push_back(Entry(result_attrs["branch_mispredict"], Variant(branch_mispredict)));
        ret.push_back(Entry(result_attrs["machine_clears"], Variant(1.0 - branch_mispredict)));

        return ret;
    }

    virtual std::size_t get_num_expected_bad_speculation() const override { return 2; }
};

class SapphireRapidsTopdown : public TopdownCalculator
{
public:

    SapphireRapidsTopdown(IntelTopdownLevel level)
        : TopdownCalculator(
            level,
            // top_counters
            "perf::slots"
            ",perf::topdown-retiring"
            ",perf::topdown-bad-spec"
            ",perf::topdown-fe-bound"
            ",perf::topdown-be-bound"
            ",INT_MISC:UOP_DROPPING",
            // all_counters
            "perf::slots"
            ",perf::topdown-retiring"
            ",perf::topdown-bad-spec"
            ",perf::topdown-fe-bound"
            ",perf::topdown-be-bound"
            ",INT_MISC:UOP_DROPPING"
            ",perf_raw::r8400"  // topdown-heavy-ops
            ",perf_raw::r8500"  // topdown-br-mispredict
            ",perf_raw::r8600"  // topdown-fetch-lat
            ",perf_raw::r8700", // topdown-mem-bound
            // res_top
            { "retiring", "backend_bound", "frontend_bound", "bad_speculation" },
            // res_all
            { "retiring",
              "backend_bound",
              "frontend_bound",
              "bad_speculation",
              "branch_mispredict",
              "machine_clears",
              "frontend_latency",
              "frontend_bandwidth",
              "memory_bound",
              "core_bound",
              "light_ops",
              "heavy_ops" }
        )
    {}

    virtual ~SapphireRapidsTopdown() = default;

    virtual std::vector<Entry> compute_toplevel(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        // Get PAPI metrics for toplevel calculations
        Variant v_slots_or_info_thread_slots = get_val_from_rec(rec, "perf::slots");
        Variant v_retiring                   = get_val_from_rec(rec, "perf::topdown-retiring");
        Variant v_bad_spec                   = get_val_from_rec(rec, "perf::topdown-bad-spec");
        Variant v_fe_bound                   = get_val_from_rec(rec, "perf::topdown-fe-bound");
        Variant v_be_bound                   = get_val_from_rec(rec, "perf::topdown-be-bound");
        Variant v_int_misc_uop_dropping      = get_val_from_rec(rec, "INT_MISC:UOP_DROPPING");

        // Check if any Variant is empty (use .empty())
        bool is_incomplete = v_fe_bound.empty() || v_be_bound.emtpy() || v_bad_spec.empty() || v_retiring.empty()
                             || v_int_misc_uop_dropping.empty() || v_slots_or_info_thread_slots.empty();
        // Check if all Variants are greater than 0 when casted to doubles (use
        // .to_double())
        bool is_nonzero = v_fe_bound.to_double() > 0.0 && v_be_bound.to_double() > 0.0 && v_bad_spec.to_double() > 0.0
                          && v_retiring.to_double() > 0.0 && v_int_misc_uop_dropping.to_double() > 0.0
                          && v_slots_or_info_thread_slots.to_double() > 0.0;

        // Check if bad values were obtained
        if (is_incomplete || !is_nonzero)
            return ret;

        // Perform toplevel calcs
        double toplevel_sum =
            (v_retiring.to_double() + v_bad_spec.to_double() + v_fe_bound.to_double() + v_be_bound.to_double());

        double retiring = (v_retiring.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
        double frontend_bound = (v_fe_bound.to_double() / toplevel_sum)
                                - (v_int_misc_uop_dropping.to_double() / v_slots_or_info_thread_slots.to_double());
        double backend_bound = (v_be_bound.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
        double bad_speculation = std::max(1.0 - (frontend_bound + backend_bound + retiring), 0.0);

        // Add toplevel metrics to vector of Entry
        ret.reserve(4);
        ret.push_back(Entry(m_result_attrs["retiring"], Variant(std::max(retiring, 0.0))));
        ret.push_back(Entry(m_result_attrs["backend_bound"], Variant(std::max(backend_bound, 0.0))));
        ret.push_back(Entry(m_result_attrs["frontend_bound"], Variant(std::max(frontend_bound, 0.0))));
        ret.push_back(Entry(m_result_attrs["bad_speculation"], Variant(std::max(bad_speculation, 0.0))));

        return ret;
    }

    virtual std::size_t get_num_expected_toplevel() const override { return 4; }

    virtual std::vector<Entry> compute_retiring(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        // Get PAPI metrics for toplevel calculations
        Variant v_slots_or_info_thread_slots = get_val_from_rec(rec, "perf::slots");
        Variant v_retiring                   = get_val_from_rec(rec, "perf::topdown-retiring");
        Variant v_bad_spec                   = get_val_from_rec(rec, "perf::topdown-bad-spec");
        Variant v_fe_bound                   = get_val_from_rec(rec, "perf::topdown-fe-bound");
        Variant v_be_bound                   = get_val_from_rec(rec, "perf::topdown-be-bound");
        Variant v_heavy_ops                  = get_val_from_rec(rec, "perf_raw::r8400");

        // Check if any Variant is empty (use .empty())
        bool is_incomplete = v_fe_bound.empty() || v_be_bound.emtpy() || v_bad_spec.empty() || v_retiring.empty()
                             || v_slots_or_info_thread_slots.empty() || v_heavy_ops.empty();

        // Check if bad values were obtained
        if (is_incomplete)
            return ret;

        double toplevel_sum =
            (v_retiring.to_double() + v_bad_spec.to_double() + v_fe_bound.to_double() + v_be_bound.to_double());
        // Copied from compute_toplevel
        double retiring = (v_retiring.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());

        double heavy_ops = (v_heavy_ops.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
        double light_ops = std::max(0.0, retiring - heavy_ops);

        // Add toplevel metrics to vector of Entry
        ret.reserve(2);
        ret.push_back(Entry(m_result_attrs["heavy_ops"], Variant(std::max(heavy_ops, 0.0))));
        ret.push_back(Entry(m_result_attrs["light_ops"], Variant(std::max(light_ops, 0.0))));

        return ret;
    }

    virtual std::size_t get_num_expected_retiring() const override { return 2; }

    virtual std::vector<Entry> compute_backend_bound(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        // Get PAPI metrics for toplevel calculations
        Variant v_slots_or_info_thread_slots = get_val_from_rec(rec, "perf::slots");
        Variant v_retiring                   = get_val_from_rec(rec, "perf::topdown-retiring");
        Variant v_bad_spec                   = get_val_from_rec(rec, "perf::topdown-bad-spec");
        Variant v_fe_bound                   = get_val_from_rec(rec, "perf::topdown-fe-bound");
        Variant v_be_bound                   = get_val_from_rec(rec, "perf::topdown-be-bound");
        Variant v_memory_bound               = get_val_from_rec(rec, "perf_raw::r8700");

        // Check if any Variant is empty (use .empty())
        bool is_incomplete = v_fe_bound.empty() || v_be_bound.emtpy() || v_bad_spec.empty() || v_retiring.empty()
                             || v_slots_or_info_thread_slots.empty() || v_memory_bound.empty();

        // Check if bad values were obtained
        if (is_incomplete)
            return ret;

        double toplevel_sum =
            (v_retiring.to_double() + v_bad_spec.to_double() + v_fe_bound.to_double() + v_be_bound.to_double());
        // Copied from compute_toplevel
        double backend_bound = (v_be_bound.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());

        double memory_bound =
            (v_memory_bound.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
        double core_bound = std::max(0.0, backend_bound - memory_bound);

        // Add toplevel metrics to vector of Entry
        ret.reserve(2);
        ret.push_back(Entry(m_result_attrs["memory_bound"], Variant(std::max(memory_bound, 0.0))));
        ret.push_back(Entry(m_result_attrs["core_bound"], Variant(std::max(core_bound, 0.0))));

        return ret;
    }

    virtual std::size_t get_num_expected_backend_bound() const override { return 2; }

    virtual std::vector<Entry> compute_frontend_bound(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        // Get PAPI metrics for toplevel calculations
        Variant v_slots_or_info_thread_slots = get_val_from_rec(rec, "perf::slots");
        Variant v_retiring                   = get_val_from_rec(rec, "perf::topdown-retiring");
        Variant v_bad_spec                   = get_val_from_rec(rec, "perf::topdown-bad-spec");
        Variant v_fe_bound                   = get_val_from_rec(rec, "perf::topdown-fe-bound");
        Variant v_be_bound                   = get_val_from_rec(rec, "perf::topdown-be-bound");
        Variant v_int_misc_uop_dropping      = get_val_from_rec(rec, "INT_MISC:UOP_DROPPING");
        Variant v_fetch_latency              = get_val_from_rec(rec, "perf_raw::r8600");

        // Check if any Variant is empty (use .empty())
        bool is_incomplete = v_fe_bound.empty() || v_be_bound.empty() || v_bad_spec.empty() || v_retiring.empty()
                             || v_int_misc_uop_dropping.empty() || v_slots_or_info_thread_slots.empty()
                             || v_fetch_latency.empty();

        // Check if bad values were obtained
        if (is_incomplete)
            return ret;

        double toplevel_sum =
            (v_retiring.to_double() + v_bad_spec.to_double() + v_fe_bound.to_double() + v_be_bound.to_double());
        // Copied from compute_toplevel
        double frontend_bound = (v_fe_bound.to_double() / toplevel_sum)
                                - (v_int_misc_uop_dropping.to_double() / v_slots_or_info_thread_slots.to_double());

        double fetch_latency = (v_fetch_latency.to_double() / toplevel_sum)
                               - (v_int_misc_uop_dropping.to_double() / v_slots_or_info_thread_slots.to_double());

        double fetch_bandwidth = std::max(0.0, frontend_bound - fetch_latency);

        // Add toplevel metrics to vector of Entry
        ret.reserve(2);
        ret.push_back(Entry(m_result_attrs["frontend_latency"], Variant(std::max(fetch_latency, 0.0))));
        ret.push_back(Entry(m_result_attrs["frontend_bandwidth"], Variant(std::max(fetch_bandwidth, 0.0))));

        return ret;
    }

    virtual std::size_t get_num_expected_frontend_bound() const override { return 2; }

    virtual std::vector<Entry> compute_bad_speculation(const std::vector<Entry>& rec) override
    {
        std::vector<Entry> ret;

        // Get PAPI metrics for toplevel calculations
        Variant v_slots_or_info_thread_slots = get_val_from_rec(rec, "perf::slots");
        Variant v_retiring                   = get_val_from_rec(rec, "perf::topdown-retiring");
        Variant v_bad_spec                   = get_val_from_rec(rec, "perf::topdown-bad-spec");
        Variant v_fe_bound                   = get_val_from_rec(rec, "perf::topdown-fe-bound");
        Variant v_be_bound                   = get_val_from_rec(rec, "perf::topdown-be-bound");
        Variant v_int_misc_uop_dropping      = get_val_from_rec(rec, "INT_MISC:UOP_DROPPING");
        Variant v_branch_mispredict          = get_val_from_rec(rec, "perf_raw::r8500");

        // Check if any Variant is empty (use .empty())
        bool is_incomplete = v_fe_bound.empty() || v_be_bound.emtpy() || v_bad_spec.empty() || v_retiring.empty()
                             || v_int_misc_uop_dropping.empty() || v_slots_or_info_thread_slots.empty()
                             || v_branch_mispredict.empty();

        // Check if bad values were obtained
        if (is_incomplete)
            return ret;

        // Perform toplevel calcs
        double toplevel_sum =
            (v_retiring.to_double() + v_bad_spec.to_double() + v_fe_bound.to_double() + v_be_bound.to_double());

        double retiring = (v_retiring.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
        double frontend_bound = (v_fe_bound.to_double() / toplevel_sum)
                                - (v_int_misc_uop_dropping.to_double() / v_slots_or_info_thread_slots.to_double());
        double backend_bound = (v_be_bound.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
        double bad_speculation = std::max(1.0 - (frontend_bound + backend_bound + retiring), 0.0);

        double branch_mispredict =
            (v_branch_mispredict.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
        double machine_clears = std::max(0.0, bad_speculation - branch_mispredict);

        // Add toplevel metrics to vector of Entry
        ret.reserve(2);
        ret.push_back(Entry(m_result_attrs["branch_mispredict"], Variant(std::max(branch_mispredict, 0.0))));
        ret.push_back(Entry(m_result_attrs["machine_clears"], Variant(std::max(machine_clears, 0.0))));

        return ret;
    }

    virtual std::size_t get_num_expected_bad_speculation() const override { return 2; }
};

class IntelTopdown
{
    unsigned num_top_computed;
    unsigned num_top_skipped;
    unsigned num_be_computed;
    unsigned num_be_skipped;
    unsigned num_fe_computed;
    unsigned num_fe_skipped;
    unsigned num_bsp_computed;
    unsigned num_bsp_skipped;
    unsigned num_ret_computed;
    unsigned num_ret_skipped;

    IntelTopdownLevel m_level;

    TopdownCalculator* m_calculator;

    bool find_counter_attrs(CaliperMetadataAccessInterface& db) { return m_calculator->find_counter_attrs(db); }

    void make_result_attrs(CaliperMetadataAccessInterface& db) { m_calculator->make_result_attrs(db); }

    void postprocess_snapshot_cb(std::vector<Entry>& rec)
    {
        std::vector<Entry> result = m_calculator->compute_toplevel(rec);

        if (result.size() != m_calculator->get_num_expected_toplevel()) {
            ++num_top_skipped;
        } else {
            rec.insert(rec.end(), result.begin(), result.end());
            ++num_top_computed;
        }

        if (m_level == All) {
            result = m_calculator->compute_backend_bound(rec);

            if (result.size() != m_calculator->get_num_expected_backend_bound()) {
                ++num_be_skipped;
            } else {
                rec.insert(rec.end(), result.begin(), result.end());
                ++num_be_computed;
            }

            result = m_calculator->compute_frontend_bound(rec);

            if (result.size() != m_calculator->get_num_expected_frontend_bound()) {
                ++num_fe_skipped;
            } else {
                rec.insert(rec.end(), result.begin(), result.end());
                ++num_fe_computed;
            }

            result = m_calculator->compute_bad_speculation(rec);

            if (result.size() != m_calculator->get_num_expected_bad_speculation()) {
                ++num_bsp_skipped;
            } else {
                rec.insert(rec.end(), result.begin(), result.end());
                ++num_bsp_computed;
            }

            result = m_calculator->compute_retiring(rec);

            if (result.size() != m_calculator->get_num_expected_retiring()) {
                ++num_ret_skipped;
            } else {
                rec.insert(rec.end(), result.begin(), result.end());
                ++num_ret_computed;
            }
        }
    }

    void finish_cb(Caliper* c, Channel* channel)
    {
        Log(1).stream() << channel->name() << ": topdown: Computed topdown metrics for " << num_top_computed
                        << " records, skipped " << num_top_skipped << std::endl;

        if (Log::verbosity() >= 2) {
            Log(2).stream() << channel->name() << ": topdown: Records processed per topdown level: "
                            << "\n  top:      " << num_top_computed << " computed, " << num_top_skipped << " skipped,"
                            << "\n  bad spec: " << num_bsp_computed << " computed, " << num_bsp_skipped << " skipped,"
                            << "\n  frontend: " << num_bsp_computed << " computed, " << num_bsp_skipped << " skipped,"
                            << "\n  backend:  " << num_bsp_computed << " computed, " << num_bsp_skipped << " skipped."
                            << std::endl;

            const std::map<std::string, int>& counters_not_found = m_calculator->get_counters_not_found();

            if (!counters_not_found.empty()) {
                std::ostringstream os;
                for (auto& p : counters_not_found)
                    os << "\n  " << p.first << ": " << p.second;
                Log(2).stream() << channel->name() << ": topdown: Counters not found:" << os.str() << std::endl;
            }
        }
    }

    explicit IntelTopdown(TopdownCalculator* calculator)
        : num_top_computed(0),
          num_top_skipped(0),
          num_be_computed(0),
          num_be_skipped(0),
          num_fe_computed(0),
          num_fe_skipped(0),
          num_bsp_computed(0),
          num_bsp_skipped(0),
          m_level(calculator->get_level()),
          m_calculator(calculator)
    {}

    ~IntelTopdown()
    {
        if (m_calculator != nullptr) {
            delete m_calculator;
        }
    }

public:

    static const char* s_spec;

    static void intel_topdown_register(Caliper* c, Channel* channel)
    {
        IntelTopdownLevel level = Top;

        auto        config = services::init_config_from_spec(channel->config(), s_spec);
        std::string lvlcfg = config.get("level").to_string();

        if (lvlcfg == "all") {
            level = All;
        } else if (lvlcfg != "top") {
            Log(0).stream() << channel->name() << ": topdown: Unknown level \"" << lvlcfg << "\", skipping topdown"
                            << std::endl;
            return;
        }

        TopdownCalculator* calculator;

#if defined(CALIPER_HAVE_ARCH)
        if (std::string(CALIPER_HAVE_ARCH) == "sapphirerapids") {
            calculator = new SapphireRapidsTopdown(level);
        } else {
#endif
            calculator = new HaswellTopdown(level); // Default type of calculation
#if defined(CALIPER_HAVE_ARCH)
        }
#endif

        channel->config().set("CALI_PAPI_COUNTERS", calculator->get_counters());

        if (!cali::services::register_service(c, channel, "papi")) {
            Log(0).stream() << channel->name() << ": topdown: Unable to register papi service, skipping topdown"
                            << std::endl;
            return;
        }

        IntelTopdown* instance = new IntelTopdown(calculator);

        channel->events().pre_flush_evt.connect([instance](Caliper* c, Channel* channel, SnapshotView) {
            if (instance->find_counter_attrs(*c))
                instance->make_result_attrs(*c);
            else
                Log(0).stream() << channel->name() << ": topdown: Could not find counter attributes!" << std::endl;
        });
        channel->events().postprocess_snapshot.connect([instance](Caliper*, Channel*, std::vector<Entry>& rec) {
            instance->postprocess_snapshot_cb(rec);
        });
        channel->events().finish_evt.connect([instance](Caliper* c, Channel* channel) {
            instance->finish_cb(c, channel);
            delete instance;
        });

        Log(1).stream() << channel->name() << ": Registered topdown service. Level: " << lvlcfg << "." << std::endl;
    }
};

const char* IntelTopdown::s_spec = R"json(
{   "name": "topdown",
    "description": "Record PAPI counters and compute top-down analysis for Intel CPUs",
    "config": [
        {   "name": "level",
            "description": "Top-down analysis level to compute ('all' or 'top')",
            "type": "string",
            "value": "top"
        }
    ]
}
)json";

} // namespace

namespace cali

{

CaliperService topdown_service { ::IntelTopdown::s_spec, ::IntelTopdown::intel_topdown_register };
}
