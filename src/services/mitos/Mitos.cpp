///@file  Mitos.cpp
///@brief Mitos provider for caliper records

#include "../CaliperService.h"

#include <Caliper.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <chrono>
#include <string>
#include <type_traits>

using namespace cali;
using namespace std;

#include <Mitos.h>

namespace 
{

Attribute address_attr        { Attribute::invalid } ;

ConfigSet config;

bool      record_address { false };

static const ConfigSet::Entry s_configdata[] = {
    { "mitos", CALI_TYPE_BOOL, "false",
      "Include sampled load addresses",
      "Include sampled load addresses"
    },
    ConfigSet::Terminator
};

void push_load_sample(Caliper* c, int scope, WriteRecordFn fn) {
    // push
}

/// Initialization handler
void mitos_register(Caliper* c)
{
    record_address = config.get("address").to_bool();

    address_attr = 
        c->create_attribute("mitos.address", CALI_TYPE_ADDR, 
                            CALI_ATTR_ASVALUE | CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_SKIP_EVENTS);

    // add callback for Caliper::get_context() event
    c->events().measure.connect(&push_load_sample);

    Log(1).stream() << "Registered mitos service" << endl;
}

} // namespace


namespace cali
{
    CaliperService MitosService = { "mitos", { ::mitos_register } };
} // namespace cali
