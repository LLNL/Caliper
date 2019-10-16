// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Papi.cpp
// PAPI provider for caliper records

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/RuntimeConfig.h"
#include "caliper/common/Log.h"

#include <vector>

using namespace cali;

#include <papi.h>

namespace 
{

#define MAX_COUNTERS 32
    
struct PapiGlobalInfo {
    std::vector<cali_id_t> counter_delta_attrs;
    std::vector<int>       counter_events;
} global_info;

size_t num_failed = 0;

static const ConfigSet::Entry s_configdata[] = {
    { "counters", CALI_TYPE_STRING, "",
      "List of PAPI events to record",
      "List of PAPI events to record, separated by ','" 
    },
    ConfigSet::Terminator
};

bool init_thread_counters(bool is_signal)
{
    static thread_local bool s_thread_active;

    if (!s_thread_active)
        if (!is_signal)
            s_thread_active =
                (PAPI_start_counters(global_info.counter_events.data(),
                                     global_info.counter_events.size()) == PAPI_OK);

    return s_thread_active;    
}

void snapshot_cb(Caliper* c, Channel* chn, int scope, const SnapshotRecord*, SnapshotRecord* snapshot) {
    auto num_counters = global_info.counter_events.size();
    
    if (num_counters < 1)
        return;

    if (!init_thread_counters(c->is_signal())) {
        ++num_failed;
        return;
    }

    long long counter_values[MAX_COUNTERS];

    if (PAPI_read_counters(counter_values, num_counters) != PAPI_OK) {
        ++num_failed;
        return;
    }

    Variant data[MAX_COUNTERS];

    for (int i = 0; i < num_counters; ++i)
        data[i] = Variant(cali_make_variant_from_uint(counter_values[i]));

    snapshot->append(num_counters, global_info.counter_delta_attrs.data(), data);
}

void papi_finish(Caliper* c, Channel* chn) {
    if (num_failed > 0)
        Log(1).stream() << chn->name() << ": PAPI: Failed to read counters " << num_failed
                        << " times." << std::endl;
}

void setup_events(Caliper* c, const std::vector<std::string>& events)
{
    Attribute aggr_class_attr = c->get_attribute("class.aggregatable");
    Variant   v_true(true);

    for (const std::string& event : events) {
        int code;

        if (PAPI_event_name_to_code(const_cast<char*>(event.c_str()), &code) != PAPI_OK) {
            Log(0).stream() << "Unable to register PAPI counter \"" << event << '"' << std::endl;
            continue;
        }

        // check if we have this event already
        if (std::find(global_info.counter_events.begin(), global_info.counter_events.end(), 
                      code) != global_info.counter_events.end())
            continue;

        if (global_info.counter_events.size() < MAX_COUNTERS) {
            Attribute delta_attr =
                c->create_attribute(std::string("papi.")+event, CALI_TYPE_UINT,
                                    CALI_ATTR_ASVALUE      | 
                                    CALI_ATTR_SCOPE_THREAD | 
                                    CALI_ATTR_SKIP_EVENTS,
                                    1, &aggr_class_attr, &v_true);

            global_info.counter_events.push_back(code);
            global_info.counter_delta_attrs.push_back(delta_attr.id());
        } else
            Log(0).stream() << "Maximum number of PAPI counters exceeded; dropping \"" 
                            << event << '"' << std::endl;
    }
}

// Initialization handler
void papi_register(Caliper* c, Channel* chn) {
    static bool papi_is_enabled = false;
    static std::string papi_channel_name;
        
    if (papi_is_enabled) {
        Log(0).stream() << chn->name() << ": PAPI: Cannot enable papi service twice!"
                        << " PAPI was already enabled in channel "
                        << papi_channel_name << std::endl;

        return;
    }

    papi_is_enabled      = true;
    papi_channel_name = chn->name();
    
    int ret = PAPI_library_init(PAPI_VER_CURRENT);
    
    if (ret != PAPI_VER_CURRENT && ret > 0) {
        Log(0).stream() << "PAPI version mismatch: found " 
                        << ret << ", expected " << PAPI_VER_CURRENT << std::endl;
        return;
    }        

    PAPI_thread_init(pthread_self);
    
    if (PAPI_is_initialized() == PAPI_NOT_INITED) {
        Log(0).stream() << "PAPI library could not be initialized" << std::endl;
        return;
    }

    ConfigSet config = chn->config().init("papi", s_configdata);

    setup_events(c, config.get("counters").to_stringlist());

    if (global_info.counter_events.size() < 1) {
        Log(1).stream() << chn->name() << ": PAPI: No counters registered, dropping PAPI service" << std::endl;
        return;
    }

    chn->events().post_init_evt.connect(
        [](Caliper* c, Channel* chn){
            init_thread_counters(c->is_signal());
        });
    chn->events().create_thread_evt.connect(
        [](Caliper* c, Channel* chn){
            init_thread_counters(c->is_signal());
        });
    chn->events().finish_evt.connect(
        [](Caliper* c, Channel* chn){
            papi_finish(c, chn);
        });
    chn->events().snapshot.connect(
        [](Caliper* c, Channel* chn, int scope, const SnapshotRecord* info, SnapshotRecord* snapshot){
            snapshot_cb(c, chn, scope, info, snapshot);
        });

    Log(1).stream() << chn->name() << ": Registered PAPI service" << std::endl;
}

} // namespace


namespace cali 
{

CaliperService papi_service = { "papi", ::papi_register };

} // namespace cali
