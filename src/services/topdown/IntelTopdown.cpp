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

using namespace cali;

namespace
{

class IntelTopdown
{
    static const char* s_counter_list;

    std::map<std::string, Attribute> counter_attrs;
    std::map<std::string, Attribute> result_attrs;

    unsigned num_computed;
    unsigned num_skipped;


    Variant get_val_from_rec(const std::vector<Entry>& rec, const char* name) const {
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

        return ret;
    }

    bool find_counter_attrs(CaliperMetadataAccessInterface& db) {
        auto counters = StringConverter(s_counter_list).to_stringlist();

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
        const char* res[] = {
            "retiring", "backend_bound", "frontend_bound", "bad_speculation"
        };

        for (const char* s : res)
            result_attrs[std::string(s)] =
                db.create_attribute(std::string("topdown.")+s, CALI_TYPE_DOUBLE, CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);
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
            v_cpu_clk_unhalted_thread_p.empty() ||
            v_uops_retired_retire_slots.empty() ||
            v_uops_issued_any.empty()           ||
            v_int_misc_recovery_cycles.empty()  ||
            v_idq_uops_not_delivered_core.empty();

        double slots = 4.0 * v_cpu_clk_unhalted_thread_p.to_double();

        if (is_incomplete || !(slots > 0.0))
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
        ret.push_back(Entry(result_attrs["retiring"],     Variant(retiring)));
        ret.push_back(Entry(result_attrs["backend_bound"], Variant(backend_bound)));
        ret.push_back(Entry(result_attrs["frontend_bound"], Variant(frontend_bound)));
        ret.push_back(Entry(result_attrs["bad_speculation"], Variant(bad_speculation)));

        return ret;
    }

    void postprocess_snapshot_cb(std::vector<Entry>& rec) {
        std::vector<Entry> result = compute_toplevel(rec);

        if (result.empty())
            ++num_skipped;
        else {
            rec.insert(rec.end(), result.begin(), result.end());
            ++num_computed;
        }
    }

    void finish_cb(Caliper* c, Channel* channel) {
        Log(1).stream() << channel->name() 
                        << ": topdown: Computed topdown metrics for " << num_computed 
                        << " records, skipped " << num_skipped
                        << std::endl;
    }

    IntelTopdown() 
        : num_computed(0), num_skipped(0) 
        { }

public:

    static void intel_topdown_register(Caliper* c, Channel* channel) {
        channel->config().set("CALI_PAPI_COUNTERS", s_counter_list);

        if (!cali::services::register_service(c, channel, "papi")) {
            Log(0).stream() << channel->name() 
                            << ": topdown: Unable to register papi service, skipping topdown"
                            << std::endl;
            return;
        }

        IntelTopdown* instance = new IntelTopdown;

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
        
        Log(1).stream() << channel->name() << ": Registered topdown service" << std::endl;
    }
};

const char* IntelTopdown::s_counter_list = 
    "CPU_CLK_THREAD_UNHALTED:THREAD_P,"
    "UOPS_RETIRED:RETIRE_SLOTS,"
    "UOPS_ISSUED:ANY,"
    "INT_MISC:RECOVERY_CYCLES,"
    "IDQ_UOPS_NOT_DELIVERED:CORE";

}

namespace cali
{

CaliperService topdown_service { "topdown", ::IntelTopdown::intel_topdown_register };

}