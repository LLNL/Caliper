#include "HaswellTopdown.h"

#include <algorithm>

namespace cali
{
namespace topdown
{

HaswellTopdown::HaswellTopdown(IntelTopdownLevel level)
    : cali::topdown::TopdownCalculator(
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

bool HaswellTopdown::check_for_disabled_multiplex() const
{
    return false;
}

std::vector<Entry> HaswellTopdown::compute_toplevel(const std::vector<Entry>& rec)
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

std::size_t HaswellTopdown::get_num_expected_toplevel() const
{
    return 4;
}

std::vector<Entry> HaswellTopdown::compute_retiring(const std::vector<Entry>& rec)
{
    return {};
}

std::size_t HaswellTopdown::get_num_expected_retiring() const
{
    return 0;
}

std::vector<Entry> HaswellTopdown::compute_backend_bound(const std::vector<Entry>& rec)
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
        (v_cycle_activity_stalls_ldm_pending.to_double() - v_cycle_activity_stalls_l1d_pending.to_double()) / clocks;
    double l2_bound =
        (v_cycle_activity_stalls_l1d_pending.to_double() - v_cycle_activity_stalls_l2_pending.to_double()) / clocks;
    double l3_bound = v_cycle_activity_stalls_l2_pending.to_double() * l3_hit_fraction / clocks;

    ret.reserve(6);
    ret.push_back(Entry(m_result_attrs["memory_bound"], Variant(memory_bound)));
    ret.push_back(Entry(m_result_attrs["core_bound"], Variant(be_bound_at_exe - memory_bound)));
    ret.push_back(Entry(m_result_attrs["ext_mem_bound"], Variant(ext_mem_bound)));
    ret.push_back(Entry(m_result_attrs["l1_bound"], Variant(l1_bound)));
    ret.push_back(Entry(m_result_attrs["l2_bound"], Variant(l2_bound)));
    ret.push_back(Entry(m_result_attrs["l3_bound"], Variant(l3_bound)));

    return ret;
}

std::size_t HaswellTopdown::get_num_expected_backend_bound() const
{
    return 6;
}

std::vector<Entry> HaswellTopdown::compute_frontend_bound(const std::vector<Entry>& rec)
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
    ret.push_back(Entry(m_result_attrs["frontend_latency"], Variant(fe_latency)));
    ret.push_back(Entry(m_result_attrs["frontend_bandwidth"], Variant(1.0 - fe_latency)));

    return ret;
}

std::size_t HaswellTopdown::get_num_expected_frontend_bound() const
{
    return 2;
}

std::vector<Entry> HaswellTopdown::compute_bad_speculation(const std::vector<Entry>& rec)
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
    ret.push_back(Entry(m_result_attrs["branch_mispredict"], Variant(branch_mispredict)));
    ret.push_back(Entry(m_result_attrs["machine_clears"], Variant(1.0 - branch_mispredict)));

    return ret;
}

std::size_t HaswellTopdown::get_num_expected_bad_speculation() const
{
    return 2;
}

} // namespace topdown
} // namespace cali