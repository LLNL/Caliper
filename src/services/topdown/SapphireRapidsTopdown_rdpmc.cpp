#include "SapphireRapidsTopdown.h"

#include "../Services.h"

#include "caliper/common/Log.h"

#include <algorithm>

#define RETIRING_OFFSET 0
#define BAD_SPEC_OFFSET 1
#define FE_BOUND_OFFSET 2
#define BE_BOUND_OFFSET 3

#define HEAVY_OPS_OFFSET 4
#define BR_MISPRED_OFFSET 5
#define FETCH_LAT_OFFSET 6
#define MEM_BOUND_OFFSET 7

static double get_tma_percent_from_rdpmc_value(uint64_t rdpmc_value, uint64_t offset)
{
    return (double) ((rdpmc_value >> (offset * 8)) & 0xff) / 0xff;
}

namespace cali
{
namespace topdown
{

SapphireRapidsTopdown::SapphireRapidsTopdown(IntelTopdownLevel level)
    : cali::topdown::TopdownCalculator(
        level,
        // top_counters
        "perf::slots"
        ",perf::topdown-retiring",
        // all_counters
        "perf::slots"
        ",perf::topdown-retiring",
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
    Variant v_tma_metrics                = get_val_from_rec(rec, "perf::topdown-retiring");

    // Check if any Variant is empty (use .empty())
    bool is_incomplete = v_tma_metrics.empty() || v_slots_or_info_thread_slots.empty();
    // Check if all Variants are greater than 0 when casted to doubles (use
    // .to_double())
    bool is_nonzero = v_tma_metrics.to_uint() > 0;

    // Check if bad values were obtained
    if (is_incomplete || !is_nonzero)
        return ret;

    uint64_t tma_metric_papi_rdpmc = v_tma_metrics.to_uint();

    double retiring        = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, RETIRING_OFFSET);
    double frontend_bound  = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, FE_BOUND_OFFSET);
    double backend_bound   = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, BE_BOUND_OFFSET);
    double bad_speculation = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, BAD_SPEC_OFFSET);

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
    Variant v_tma_metrics                = get_val_from_rec(rec, "perf::topdown-retiring");

    // Check if any Variant is empty (use .empty())
    bool is_incomplete = v_tma_metrics.empty() || v_slots_or_info_thread_slots.empty();

    // Check if bad values were obtained
    if (is_incomplete)
        return ret;

    uint64_t tma_metric_papi_rdpmc = v_tma_metrics.to_uint();

    double retiring  = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, RETIRING_OFFSET);
    double heavy_ops = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, HEAVY_OPS_OFFSET);
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
    Variant v_tma_metrics                = get_val_from_rec(rec, "perf::topdown-retiring");

    // Check if any Variant is empty (use .empty())
    bool is_incomplete = v_tma_metrics.empty() || v_slots_or_info_thread_slots.empty();

    // Check if bad values were obtained
    if (is_incomplete)
        return ret;

    uint64_t tma_metric_papi_rdpmc = v_tma_metrics.to_uint();

    double backend_bound = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, BE_BOUND_OFFSET);
    double memory_bound  = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, MEM_BOUND_OFFSET);
    double core_bound    = std::max(0.0, backend_bound - memory_bound);

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
    Variant v_tma_metrics                = get_val_from_rec(rec, "perf::topdown-retiring");

    // Check if any Variant is empty (use .empty())
    bool is_incomplete = v_tma_metrics.empty() || v_slots_or_info_thread_slots.empty();

    // Check if bad values were obtained
    if (is_incomplete)
        return ret;

    uint64_t tma_metric_papi_rdpmc = v_tma_metrics.to_uint();

    double frontend_bound  = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, FE_BOUND_OFFSET);
    double fetch_latency   = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, FETCH_LAT_OFFSET);
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
    Variant v_tma_metrics                = get_val_from_rec(rec, "perf::topdown-retiring");

    // Check if any Variant is empty (use .empty())
    bool is_incomplete = v_tma_metrics.empty() || v_slots_or_info_thread_slots.empty();

    // Check if bad values were obtained
    if (is_incomplete)
        return ret;

    uint64_t tma_metric_papi_rdpmc = v_tma_metrics.to_uint();

    double bad_speculation   = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, BAD_SPEC_OFFSET);
    double branch_mispredict = get_tma_percent_from_rdpmc_value(tma_metric_papi_rdpmc, BR_MISPRED_OFFSET);
    double machine_clears    = std::max(0.0, bad_speculation - branch_mispredict);

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