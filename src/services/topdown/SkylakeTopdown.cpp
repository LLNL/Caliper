#include "SkylakeTopdown.h"

#include "../Services.h"

#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

namespace cali
{
namespace topdown
{

SkylakeTopdown::SkylakeTopdown(IntelTopdownLevel level)
    : cali::topdown::TopdownCalculator(
        level,
        // top_counters
        "IDQ_UOPS_NOT_DELIVERED:CORE"
        ",UOPS_ISSUED:ANY"
        ",UOPS_RETIRED:RETIRE_SLOTS"
        ",INT_MISC:RECOVERY_CYCLES"
        ",CPU_CLK_UNHALTED:THREAD",
        // all_counters
        "IDQ_UOPS_NOT_DELIVERED:CORE"
        ",UOPS_ISSUED:ANY"
        ",UOPS_RETIRED:RETIRE_SLOTS"
        ",INT_MISC:RECOVERY_CYCLES"
        ",CPU_CLK_UNHALTED:THREAD",
        // res_top
        { "retiring", "backend_bound", "frontend_bound", "bad_speculation" },
        // res_all
        { "retiring", "backend_bound", "frontend_bound", "bad_speculation" }
    )
{}

bool SkylakeTopdown::setup_config(Caliper& c, Channel& channel) const
{
    channel.config().set("CALI_PAPI_COUNTERS", m_level == All ? m_all_counters : m_top_counters);
    channel.config().set("CALI_PAPI_ENABLE_MULTIPLEXING", "true");
    if (!cali::services::register_service(&c, &channel, "papi")) {
        Log(0).stream() << channel.name() << ": topdown: Unable to register papi service, skipping topdown"
                        << std::endl;
        return false;
    }
    return true;
}

std::vector<Entry> SkylakeTopdown::compute_toplevel(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_idq_uops_not_delivered_core = get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CORE");
    Variant v_uops_issued_any             = get_val_from_rec(rec, "UOPS_ISSUED:ANY");
    Variant v_uops_retired_retire_slots   = get_val_from_rec(rec, "UOPS_RETIRED:RETIRE_SLOTS");
    Variant v_int_misc_recovery_cycles    = get_val_from_rec(rec, "INT_MISC:RECOVERY_CYCLES");
    Variant v_cpu_clk_unhalted_thread     = get_val_from_rec(rec, "CPU_CLK_UNHALTED:THREAD");

    bool is_incomplete = v_idq_uops_not_delivered_core.empty() || v_uops_issued_any.empty()
                         || v_uops_retired_retire_slots.empty() || v_int_misc_recovery_cycles.empty()
                         || v_cpu_clk_unhalted_thread.empty();
    bool is_nonzero = v_idq_uops_not_delivered_core.to_double() > 0.0 && v_uops_issued_any.to_double() > 0.0
                      && v_uops_retired_retire_slots.to_double() > 0.0 && v_int_misc_recovery_cycles.to_double() > 0.0
                      && v_idq_uops_not_delivered_core.to_double() > 0.0;

    double thread_slots = 4 * v_cpu_clk_unhalted_thread.to_double();

    if (is_incomplete || is_nonzero || thread_slots < 1.0) {
        return ret;
    }

    double frontend_bound  = v_idq_uops_not_delivered_core.to_double() / thread_slots;
    double bad_speculation = (v_uops_issued_any.to_double() - v_uops_retired_retire_slots.to_double()
                              + 4 * v_int_misc_recovery_cycles.to_double())
                             / thread_slots;
    double backend_bound =
        1 - frontend_bound
        - (v_uops_issued_any.to_double() + 4 * v_int_misc_recovery_cycles.to_double()) / thread_slots;
    double retiring = v_uops_retired_retire_slots.to_double() / thread_slots;

    ret.reserve(4);
    ret.push_back(Entry(m_result_attrs["retiring"], Variant(std::max(retiring, 0.0))));
    ret.push_back(Entry(m_result_attrs["backend_bound"], Variant(std::max(backend_bound, 0.0))));
    ret.push_back(Entry(m_result_attrs["frontend_bound"], Variant(std::max(frontend_bound, 0.0))));
    ret.push_back(Entry(m_result_attrs["bad_speculation"], Variant(std::max(bad_speculation, 0.0))));

    return ret;
}

std::size_t SkylakeTopdown::get_num_expected_toplevel() const
{
    return 4;
}

std::vector<Entry> SkylakeTopdown::compute_retiring(const std::vector<Entry>& rec)
{
    return {};
}

std::size_t SkylakeTopdown::get_num_expected_retiring() const
{
    return 0;
}

std::vector<Entry> SkylakeTopdown::compute_backend_bound(const std::vector<Entry>& rec)
{
    return {};
}

std::size_t SkylakeTopdown::get_num_expected_backend_bound() const
{
    return 0;
}

std::vector<Entry> SkylakeTopdown::compute_frontend_bound(const std::vector<Entry>& rec)
{
    return {};
}

std::size_t SkylakeTopdown::get_num_expected_frontend_bound() const
{
    return 0;
}

std::vector<Entry> SkylakeTopdown::compute_bad_speculation(const std::vector<Entry>& rec)
{
    return {};
}

std::size_t SkylakeTopdown::get_num_expected_bad_speculation() const
{
    return 0;
}

} // namespace topdown
} // namespace cali