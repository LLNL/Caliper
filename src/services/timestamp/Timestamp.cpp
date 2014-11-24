///@file  Timestamp.cpp
///@brief Timestamp provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>

#include <Log.h>
#include <RuntimeConfig.h>

#include <chrono>
#include <string>
#include <type_traits>

using namespace cali;
using namespace std;

namespace 
{

Attribute attribute { Attribute::invalid } ;
chrono::time_point<chrono::high_resolution_clock> tstart;

const ConfigSet::Entry s_configdata[] = {
    // key, type, value, short description, long description
    { "enable",   CALI_TYPE_BOOL, "false",
      "Automatically add timestamp to context queries",
      "Automatically add timestamp to context queries"
    },
    ConfigSet::Terminator
};

/// Event callback
/// Updates timestamp on current global context
void update_time(Caliper* c, cali_id_t env) {
    auto now = chrono::high_resolution_clock::now();
    uint64_t usec = chrono::duration_cast<chrono::microseconds>(now - tstart).count();

    c->set(env, attribute, &usec, sizeof(usec));
}

/// Initialization handler
void timestamp_register(Caliper* c)
{
    if (!RuntimeConfig::init("timestamp", s_configdata).get("enable").to_bool())
        return;

    // set start time and create time attribute
    tstart    = chrono::high_resolution_clock::now();
    attribute = 
        c->create_attribute("time(usec)", CALI_TYPE_UINT, CALI_ATTR_ASVALUE | CALI_ATTR_GLOBAL);

    // add callback for Caliper::get_context() event
    c->events().queryEvt.connect(&update_time);

    Log(2).stream() << "Registered timestamp service" << endl;
}

} // namespace


namespace cali
{
    CaliperService TimestampService = { "timestamp", { ::timestamp_register } };
} // namespace cali
