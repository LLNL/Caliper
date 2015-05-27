///@file  Papi.cpp
///@brief PAPI provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>
#include <ContextBuffer.h>
#include <SigsafeRWLock.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <unistd.h>
#include <sys/syscall.h>

#include <map>

using namespace cali;
using namespace std;

#include <papi.h>

#define NUM_COUNTERS 1

namespace 
{

Attribute ins_attr        { Attribute::invalid } ;

ConfigSet config;

bool      record_instructions { false };

int counter_events[NUM_COUNTERS] = {PAPI_TOT_INS};
long long counter_values[NUM_COUNTERS];

static const ConfigSet::Entry s_configdata[] = {
    { "papi", CALI_TYPE_BOOL, "false",
      "Include PAPI counters",
      "Include PAPI counters"
    },
    ConfigSet::Terminator
};

void push_counter(Caliper* c, int scope, WriteRecordFn fn) {
    Variant v_attr[1];
    Variant v_data[1];

    if(PAPI_accum_counters(counter_values, NUM_COUNTERS) != PAPI_OK)
    {
        Log(1).stream() << "PAPI failed to accumulate counters!" << endl;
        return;
    }

    v_attr[0] = ins_attr.id();
    v_data[0] = (uint64_t)counter_values[0];

    int               n[3] = {       0,      1,      1 };
    const Variant* data[3] = { nullptr, v_attr, v_data };

    fn(ContextRecord::record_descriptor(), n, data);
}

void papi_init(Caliper* c) {
    if(PAPI_start_counters(counter_events,NUM_COUNTERS) != PAPI_OK)
    {
        Log(1).stream() << "PAPI counters failed to initialize!" << endl;
    }
    else
    {
        Log(1).stream() << "PAPI counters initialized successfully" << endl;
    }
}

void papi_finish(Caliper* c) {
    if(PAPI_stop_counters(counter_values,NUM_COUNTERS) != PAPI_OK)
    {
        Log(1).stream() << "PAPI counters failed to stop!" << endl;
    }
    else
    {
        Log(1).stream() << "PAPI counters stopped successfully" << endl;
    }
}

// Initialization handler
void papi_register(Caliper* c) {
    record_instructions = config.get("instructions").to_bool();

    ins_attr = c->create_attribute("papi.instructions", CALI_TYPE_UINT, 
                                       CALI_ATTR_ASVALUE 
                                       | CALI_ATTR_SCOPE_THREAD 
                                       | CALI_ATTR_SKIP_EVENTS);

    // add callback for Caliper::get_context() event
    c->events().post_init_evt.connect(&papi_init);
    c->events().finish_evt.connect(&papi_finish);
    c->events().measure.connect(&push_counter);

    Log(1).stream() << "Registered papi service" << endl;
}

} // namespace


namespace cali 
{
    CaliperService PapiService = { "papi", { ::papi_register } };
} // namespace cali
