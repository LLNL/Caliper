#include "SapphireRapidsTopdown.h"

#include "../Services.h"

#include "caliper/common/Log.h"

#include <algorithm>

namespace cali
{
namespace topdown
{

SapphireRapidsTopdown::SapphireRapidsTopdown(IntelTopdownLevel level)
    : cali::topdown::TopdownCalculator(
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

bool SapphireRapidsTopdown::setup_config(Caliper& c, Channel& channel) const
{
    channel.config().set("CALI_PAPI_COUNTERS", m_level == All ? m_all_counters : m_top_counters);

    if (!cali::services::register_service(&c, &channel, "papi")) {
        Log(0).stream() << channel.name() << ": topdown: Unable to register papi service, skipping topdown"
            << std::endl;
        return false;
    }

    return true;
}

std::vector<Entry> SapphireRapidsTopdown::compute_toplevel(const std::vector<Entry>& rec)
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
    bool is_incomplete = v_fe_bound.empty() || v_be_bound.empty() || v_bad_spec.empty() || v_retiring.empty()
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

    double retiring       = (v_retiring.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
    double frontend_bound = (v_fe_bound.to_double() / toplevel_sum)
                            - (v_int_misc_uop_dropping.to_double() / v_slots_or_info_thread_slots.to_double());
    double backend_bound   = (v_be_bound.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
    double bad_speculation = std::max(1.0 - (frontend_bound + backend_bound + retiring), 0.0);

    // Add toplevel metrics to vector of Entry
    ret.reserve(4);
    ret.push_back(Entry(m_result_attrs["retiring"], Variant(std::max(retiring, 0.0))));
    ret.push_back(Entry(m_result_attrs["backend_bound"], Variant(std::max(backend_bound, 0.0))));
    ret.push_back(Entry(m_result_attrs["frontend_bound"], Variant(std::max(frontend_bound, 0.0))));
    ret.push_back(Entry(m_result_attrs["bad_speculation"], Variant(std::max(bad_speculation, 0.0))));

    return ret;
}

std::size_t SapphireRapidsTopdown::get_num_expected_toplevel() const
{
    return 4;
}

std::vector<Entry> SapphireRapidsTopdown::compute_retiring(const std::vector<Entry>& rec)
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
    bool is_incomplete = v_fe_bound.empty() || v_be_bound.empty() || v_bad_spec.empty() || v_retiring.empty()
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

std::size_t SapphireRapidsTopdown::get_num_expected_retiring() const
{
    return 2;
}

std::vector<Entry> SapphireRapidsTopdown::compute_backend_bound(const std::vector<Entry>& rec)
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
    bool is_incomplete = v_fe_bound.empty() || v_be_bound.empty() || v_bad_spec.empty() || v_retiring.empty()
                         || v_slots_or_info_thread_slots.empty() || v_memory_bound.empty();

    // Check if bad values were obtained
    if (is_incomplete)
        return ret;

    double toplevel_sum =
        (v_retiring.to_double() + v_bad_spec.to_double() + v_fe_bound.to_double() + v_be_bound.to_double());
    // Copied from compute_toplevel
    double backend_bound = (v_be_bound.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());

    double memory_bound = (v_memory_bound.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
    double core_bound   = std::max(0.0, backend_bound - memory_bound);

    // Add toplevel metrics to vector of Entry
    ret.reserve(2);
    ret.push_back(Entry(m_result_attrs["memory_bound"], Variant(std::max(memory_bound, 0.0))));
    ret.push_back(Entry(m_result_attrs["core_bound"], Variant(std::max(core_bound, 0.0))));

    return ret;
}

std::size_t SapphireRapidsTopdown::get_num_expected_backend_bound() const
{
    return 2;
}

std::vector<Entry> SapphireRapidsTopdown::compute_frontend_bound(const std::vector<Entry>& rec)
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

std::size_t SapphireRapidsTopdown::get_num_expected_frontend_bound() const
{
    return 2;
}

std::vector<Entry> SapphireRapidsTopdown::compute_bad_speculation(const std::vector<Entry>& rec)
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
    bool is_incomplete = v_fe_bound.empty() || v_be_bound.empty() || v_bad_spec.empty() || v_retiring.empty()
                         || v_int_misc_uop_dropping.empty() || v_slots_or_info_thread_slots.empty()
                         || v_branch_mispredict.empty();

    // Check if bad values were obtained
    if (is_incomplete)
        return ret;

    // Perform toplevel calcs
    double toplevel_sum =
        (v_retiring.to_double() + v_bad_spec.to_double() + v_fe_bound.to_double() + v_be_bound.to_double());

    double retiring       = (v_retiring.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
    double frontend_bound = (v_fe_bound.to_double() / toplevel_sum)
                            - (v_int_misc_uop_dropping.to_double() / v_slots_or_info_thread_slots.to_double());
    double backend_bound   = (v_be_bound.to_double() / toplevel_sum) + (0 * v_slots_or_info_thread_slots.to_double());
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

std::size_t SapphireRapidsTopdown::get_num_expected_bad_speculation() const
{
    return 2;
}

} // namespace topdown
} // namespace cali