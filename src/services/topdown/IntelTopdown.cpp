// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// IntelTopdown.cpp
// Intel top-down analysis

#include "caliper/CaliperService.h"

#include "../Services.h"

#include "HaswellTopdown.h"
#include "SapphireRapidsTopdown.h"

#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/StringConverter.h"

#include "../Services.h"

#include <algorithm>
#include <sstream>

using namespace cali;

namespace
{

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

    cali::topdown::IntelTopdownLevel m_level;

    cali::topdown::TopdownCalculator* m_calculator;

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

    explicit IntelTopdown(cali::topdown::TopdownCalculator* calculator)
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

    static void intel_topdown_register(Caliper* c, Channel* channel)
    {
        cali::topdown::IntelTopdownLevel level = Top;

        auto        config = services::init_config_from_spec(channel->config(), s_spec);
        std::string lvlcfg = config.get("level").to_string();

        if (lvlcfg == "all") {
            level = All;
        } else if (lvlcfg != "top") {
            Log(0).stream() << channel->name() << ": topdown: Unknown level \"" << lvlcfg << "\", skipping topdown"
                            << std::endl;
            return;
        }

        cali::topdown::TopdownCalculator* calculator;

#if defined(CALIPER_HAVE_ARCH)
        if (std::string(CALIPER_HAVE_ARCH) == "sapphirerapids") {
            calculator = new cali::topdown::SapphireRapidsTopdown(level);
        } else {
#endif
            calculator = new cali::topdown::HaswellTopdown(level); // Default type of calculation
#if defined(CALIPER_HAVE_ARCH)
        }
#endif

        channel->config().set("CALI_PAPI_COUNTERS", calculator->get_counters());
        // Some PAPI counters for topdown (particularly on SPR) don't play nice
        // with PAPI multiplexing. Ask the TopdownCalculator class whether we need
        // to disable multiplexing for the corresponding architecture.
        channel->config().set("CALI_PAPI_DISABLE_MULTIPLEXING", calculator->check_for_disabled_multiplex());

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
