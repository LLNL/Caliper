// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// IntelTopdown.cpp
// Intel top-down analysis

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/StringConverter.h"

#include "../Services.h"

#include <algorithm>
#include <map>
#include <sstream>

using namespace cali;

namespace
{

class IntelTopdown
{
    static const char* s_top_counters;
    static const char* s_all_counters;

    static const ConfigSet::Entry s_configdata[];

    std::map<std::string, Attribute> counter_attrs;
    std::map<std::string, Attribute> result_attrs;

    std::map<std::string, int> counters_not_found;

    unsigned num_top_computed;
    unsigned num_top_skipped;
    unsigned num_be_computed;
    unsigned num_be_skipped;
    unsigned num_fe_computed;
    unsigned num_fe_skipped;
    unsigned num_bsp_computed;
    unsigned num_bsp_skipped;

    enum Level {
        All = 1,
        Top = 2
    };

    Level level;

    Variant get_val_from_rec(const std::vector<Entry>& rec, const char* name) {
        Variant ret;

        auto c_it = counter_attrs.find(name);
        if (c_it == counter_attrs.end())
            return ret;

        cali_id_t attr_id = c_it->second.id();

        auto it = std::find_if(rec.begin(), rec.end(), [attr_id](const Entry& e){
                return e.attribute() == attr_id;
            });

        if (it != rec.end())
            ret = it->value();
        else
            ++counters_not_found[std::string(name)];

        return ret;
    }

    bool find_counter_attrs(CaliperMetadataAccessInterface& db) {
        const char* list = (level == All ? s_all_counters : s_top_counters);
        auto counters = StringConverter(list).to_stringlist();

        for (const auto &s : counters) {
            Attribute attr = db.get_attribute(std::string("sum#papi.")+s);

            if (attr == Attribute::invalid)
                attr = db.get_attribute(std::string("papi.")+s);
            if (attr == Attribute::invalid) {
                Log(0).stream() << "topdown: " << s << " counter attribute not found!" << std::endl;
                return false;
            }

            counter_attrs[s] = attr;
        }

        return true;
    }

    void make_result_attrs(CaliperMetadataAccessInterface& db) {
        const char* res_top[] = {
            "retiring", "backend_bound", "frontend_bound", "bad_speculation", nullptr
        };
        const char* res_all[] = {
            "retiring", "backend_bound", "frontend_bound", "bad_speculation",
            "branch_mispredict", "machine_clears",
            "frontend_latency", "frontend_bandwidth",
            "memory_bound", "core_bound",
            "ext_mem_bound", "l1_bound", "l2_bound", "l3_bound",
            nullptr
        };

        const char** res = (level == Top ? res_top : res_all);

        for (const char** s = res; s && *s; ++s)
            result_attrs[std::string(*s)] =
                db.create_attribute(std::string("topdown.")+(*s), CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
    }

    std::vector<Entry>
    compute_toplevel(const std::vector<Entry>& rec)
    {
        std::vector<Entry> ret;

        Variant v_cpu_clk_unhalted_thread_p =
            get_val_from_rec(rec, "CPU_CLK_THREAD_UNHALTED:THREAD_P");
        Variant v_uops_retired_retire_slots =
            get_val_from_rec(rec, "UOPS_RETIRED:RETIRE_SLOTS");
        Variant v_uops_issued_any =
            get_val_from_rec(rec, "UOPS_ISSUED:ANY");
        Variant v_int_misc_recovery_cycles =
            get_val_from_rec(rec, "INT_MISC:RECOVERY_CYCLES");
        Variant v_idq_uops_not_delivered_core =
            get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CORE");

        bool is_incomplete =
            v_cpu_clk_unhalted_thread_p.empty()   ||
            v_uops_retired_retire_slots.empty()   ||
            v_uops_issued_any.empty()             ||
            v_int_misc_recovery_cycles.empty()    ||
            v_idq_uops_not_delivered_core.empty();
        bool is_nonzero =
            v_cpu_clk_unhalted_thread_p.to_double() > 0.0 &&
            v_uops_retired_retire_slots.to_double() > 0.0 &&
            v_uops_issued_any.to_double() > 0.0           &&
            v_int_misc_recovery_cycles.to_double()  > 0.0 &&
            v_idq_uops_not_delivered_core.to_double() > 0.0;

        double slots = 4.0 * v_cpu_clk_unhalted_thread_p.to_double();

        if (is_incomplete || !is_nonzero || slots < 1.0)
            return ret;

        double retiring = v_uops_retired_retire_slots.to_double() / slots;
        double bad_speculation =
            (v_uops_issued_any.to_double()
                - v_uops_retired_retire_slots.to_double()
                + 4.0 * v_int_misc_recovery_cycles.to_double()) / slots;
        double frontend_bound = v_idq_uops_not_delivered_core.to_double() / slots;
        double backend_bound =
            1.0 - (retiring + bad_speculation + frontend_bound);

        ret.reserve(4);
        ret.push_back(Entry(result_attrs["retiring"],     Variant(std::max(retiring, 0.0))));
        ret.push_back(Entry(result_attrs["backend_bound"], Variant(std::max(backend_bound, 0.0))));
        ret.push_back(Entry(result_attrs["frontend_bound"], Variant(std::max(frontend_bound, 0.0))));
        ret.push_back(Entry(result_attrs["bad_speculation"], Variant(std::max(bad_speculation, 0.0))));

        return ret;
    }

    std::vector<Entry>
    compute_backend_bound(const std::vector<Entry>& rec)
    {
        std::vector<Entry> ret;

        Variant v_cpu_clk_unhalted_thread_p =
            get_val_from_rec(rec, "CPU_CLK_THREAD_UNHALTED:THREAD_P");
        Variant v_cycle_activity_stalls_ldm_pending =
            get_val_from_rec(rec, "CYCLE_ACTIVITY:STALLS_LDM_PENDING");
        Variant v_cycle_activity_cycles_no_execute =
            get_val_from_rec(rec, "CYCLE_ACTIVITY:CYCLES_NO_EXECUTE");
        Variant v_uops_executed_core_cycles_ge_1 =
            get_val_from_rec(rec, "UOPS_EXECUTED:CORE_CYCLES_GE_1");
        Variant v_uops_executed_core_cycles_ge_2 =
            get_val_from_rec(rec, "UOPS_EXECUTED:CORE_CYCLES_GE_2");
        Variant v_mem_load_uops_retired_l3_miss =
            get_val_from_rec(rec, "MEM_LOAD_UOPS_RETIRED:L3_MISS");
        Variant v_mem_load_uops_retired_l3_hit =
            get_val_from_rec(rec, "MEM_LOAD_UOPS_RETIRED:L3_HIT");
        Variant v_cycle_activity_stalls_l2_pending =
            get_val_from_rec(rec, "CYCLE_ACTIVITY:STALLS_L2_PENDING");
        Variant v_cycle_activity_stalls_l1d_pending =
            get_val_from_rec(rec, "CYCLE_ACTIVITY:STALLS_L1D_PENDING");

        bool is_incomplete =
            v_cpu_clk_unhalted_thread_p.empty()         ||
            v_cycle_activity_stalls_ldm_pending.empty() ||
            v_cycle_activity_cycles_no_execute.empty()  ||
            v_uops_executed_core_cycles_ge_1.empty()    ||
            v_uops_executed_core_cycles_ge_2.empty()    ||
            v_mem_load_uops_retired_l3_miss.empty()     ||
            v_mem_load_uops_retired_l3_hit.empty()      ||
            v_cycle_activity_stalls_l2_pending.empty()  ||
            v_cycle_activity_stalls_l1d_pending.empty();

        double clocks = v_cpu_clk_unhalted_thread_p.to_double();

        if (is_incomplete || !(clocks > 1.0))
            return ret;

        double memory_bound =
            v_cycle_activity_stalls_ldm_pending.to_double() / clocks;
        double be_bound_at_exe =
            (v_cycle_activity_cycles_no_execute.to_double()
                + v_uops_executed_core_cycles_ge_1.to_double()
                - v_uops_executed_core_cycles_ge_2.to_double()) / clocks;
        double l3_tot =
            v_mem_load_uops_retired_l3_hit.to_double() +
                7.0 * v_mem_load_uops_retired_l3_miss.to_double();
        double l3_hit_fraction  = 0.0;
        double l3_miss_fraction = 0.0;
        if (l3_tot > 0.0) {
            l3_hit_fraction =
                v_mem_load_uops_retired_l3_hit.to_double()  / l3_tot;
            l3_miss_fraction =
                v_mem_load_uops_retired_l3_miss.to_double() / l3_tot;
        }
        double ext_mem_bound =
            v_cycle_activity_stalls_l2_pending.to_double() * l3_miss_fraction / clocks;
        double l1_bound =
            (v_cycle_activity_stalls_ldm_pending.to_double()
                - v_cycle_activity_stalls_l1d_pending.to_double()) / clocks;
        double l2_bound =
            (v_cycle_activity_stalls_l1d_pending.to_double()
                - v_cycle_activity_stalls_l2_pending.to_double()) / clocks;
        double l3_bound =
            v_cycle_activity_stalls_l2_pending.to_double() * l3_hit_fraction / clocks;

        ret.reserve(6);
        ret.push_back(Entry(result_attrs["memory_bound"], Variant(memory_bound)));
        ret.push_back(Entry(result_attrs["core_bound"], Variant(be_bound_at_exe - memory_bound)));
        ret.push_back(Entry(result_attrs["ext_mem_bound"], Variant(ext_mem_bound)));
        ret.push_back(Entry(result_attrs["l1_bound"], Variant(l1_bound)));
        ret.push_back(Entry(result_attrs["l2_bound"], Variant(l2_bound)));
        ret.push_back(Entry(result_attrs["l3_bound"], Variant(l3_bound)));

        return ret;
    }

    std::vector<Entry>
    compute_frontend_bound(const std::vector<Entry>& rec)
    {
        std::vector<Entry> ret;

        Variant v_cpu_clk_unhalted_thread_p =
            get_val_from_rec(rec, "CPU_CLK_THREAD_UNHALTED:THREAD_P");
        Variant v_idq_uops_not_delivered =
            get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CYCLES_0_UOPS_DELIV_CORE");

        bool is_incomplete =
            v_cpu_clk_unhalted_thread_p.empty() ||
            v_idq_uops_not_delivered.empty();

        double clocks = v_cpu_clk_unhalted_thread_p.to_double();
        double uops = v_idq_uops_not_delivered.to_double();

        if (is_incomplete || clocks < 1.0 || uops > clocks)
            return ret;

        double fe_latency = uops / clocks;

        ret.reserve(2);
        ret.push_back(Entry(result_attrs["frontend_latency"], Variant(fe_latency)));
        ret.push_back(Entry(result_attrs["frontend_bandwidth"], Variant(1.0 - fe_latency)));

        return ret;
    }

    std::vector<Entry>
    compute_bad_speculation(const std::vector<Entry>& rec)
    {
        std::vector<Entry> ret;

        Variant v_br_misp_retired_all_branches =
            get_val_from_rec(rec, "BR_MISP_RETIRED:ALL_BRANCHES");
        Variant v_machine_clears_count =
            get_val_from_rec(rec, "MACHINE_CLEARS:COUNT");

        bool is_incomplete =
            v_br_misp_retired_all_branches.empty() ||
            v_machine_clears_count.empty();

        double br_misp_retired_all_branches = v_br_misp_retired_all_branches.to_double();
        double machine_clears_count = v_machine_clears_count.to_double();

        if (is_incomplete || !(br_misp_retired_all_branches + machine_clears_count > 1.0))
            return ret;

        double branch_mispredict =
            br_misp_retired_all_branches / (br_misp_retired_all_branches + machine_clears_count);

        ret.reserve(2);
        ret.push_back(Entry(result_attrs["branch_mispredict"], Variant(branch_mispredict)));
        ret.push_back(Entry(result_attrs["machine_clears"], Variant(1.0 - branch_mispredict)));

        return ret;
    }

    void postprocess_snapshot_cb(std::vector<Entry>& rec) {
        std::vector<Entry> result = compute_toplevel(rec);

        if (result.empty())
            ++num_top_skipped;
        else {
            rec.insert(rec.end(), result.begin(), result.end());
            ++num_top_computed;
        }

        if (level == All) {
            result = compute_backend_bound(rec);

            if (result.empty())
                ++num_be_skipped;
            else {
                rec.insert(rec.end(), result.begin(), result.end());
                ++num_be_computed;
            }

            result = compute_frontend_bound(rec);

            if (result.empty())
                ++num_fe_skipped;
            else {
                rec.insert(rec.end(), result.begin(), result.end());
                ++num_fe_computed;
            }

            result = compute_bad_speculation(rec);

            if (result.empty())
                ++num_bsp_skipped;
            else {
                rec.insert(rec.end(), result.begin(), result.end());
                ++num_bsp_computed;
            }
        }
    }

    void finish_cb(Caliper* c, Channel* channel) {
        Log(1).stream() << channel->name()
                        << ": topdown: Computed topdown metrics for " << num_top_computed
                        << " records, skipped " << num_top_skipped
                        << std::endl;

        if (Log::verbosity() >= 2) {
            Log(2).stream() << channel->name()
                            << ": topdown: Records processed per topdown level: "
                            << "\n  top:      " << num_top_computed << " computed, " << num_top_skipped << " skipped,"
                            << "\n  bad spec: " << num_bsp_computed << " computed, " << num_bsp_skipped << " skipped,"
                            << "\n  frontend: " << num_bsp_computed << " computed, " << num_bsp_skipped << " skipped,"
                            << "\n  backend:  " << num_bsp_computed << " computed, " << num_bsp_skipped << " skipped."
                            << std::endl;

            if (!counters_not_found.empty()) {
                std::ostringstream os;
                for (auto &p : counters_not_found)
                    os << "\n  " << p.first << ": " << p.second;
                Log(2).stream() << channel->name() << ": topdown: Counters not found:"
                                << os.str()
                                << std::endl;
            }
        }
    }

    explicit IntelTopdown(Level lvl)
        : num_top_computed(0),
          num_top_skipped(0),
          num_be_computed(0),
          num_be_skipped(0),
          num_fe_computed(0),
          num_fe_skipped(0),
          num_bsp_computed(0),
          num_bsp_skipped(0),
          level(lvl)
        { }

public:

    static void intel_topdown_register(Caliper* c, Channel* channel) {
        Level level = Top;
        const char* counters = s_top_counters;

        std::string lvlcfg =
            channel->config().init("topdown", s_configdata).get("level").to_string();

        if (lvlcfg == "all") {
            level    = All;
            counters = s_all_counters;
        } else if (lvlcfg != "top") {
            Log(0).stream() << channel->name() << ": topdown: Unknown level \""
                            << lvlcfg << "\", skipping topdown" << std::endl;
            return;
        }

        channel->config().set("CALI_PAPI_COUNTERS", counters);

        if (!cali::services::register_service(c, channel, "papi")) {
            Log(0).stream() << channel->name()
                            << ": topdown: Unable to register papi service, skipping topdown"
                            << std::endl;
            return;
        }

        IntelTopdown* instance = new IntelTopdown(level);

        channel->events().pre_flush_evt.connect(
            [instance](Caliper* c, Channel* channel, const SnapshotRecord*){
                if (instance->find_counter_attrs(*c))
                    instance->make_result_attrs(*c);
                else
                    Log(0).stream() << channel->name()
                                    << ": topdown: Could not find counter attributes!"
                                    << std::endl;
            });
        channel->events().postprocess_snapshot.connect(
            [instance](Caliper*, Channel*, std::vector<Entry>& rec){
                instance->postprocess_snapshot_cb(rec);
            });
        channel->events().finish_evt.connect(
            [instance](Caliper* c, Channel* channel){
                instance->finish_cb(c, channel);
                delete instance;
            });

        Log(1).stream() << channel->name() << ": Registered topdown service. Level: " << lvlcfg << "." << std::endl;
    }
};

const char* IntelTopdown::s_top_counters =
    "CPU_CLK_THREAD_UNHALTED:THREAD_P"
    ",IDQ_UOPS_NOT_DELIVERED:CORE"
    ",INT_MISC:RECOVERY_CYCLES"
    ",UOPS_ISSUED:ANY"
    ",UOPS_RETIRED:RETIRE_SLOTS";

const char* IntelTopdown::s_all_counters =
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
    ",UOPS_RETIRED:RETIRE_SLOTS";

const ConfigSet::Entry IntelTopdown::s_configdata[] = {
    { "level", CALI_TYPE_STRING, "top",
      "Top-down analysis level to compute ('all' or 'top')",
      "Top-down analysis level to compute ('all' or 'top')"
    },
    ConfigSet::Terminator
};

}

namespace cali
{

CaliperService topdown_service { "topdown", ::IntelTopdown::intel_topdown_register };

}
