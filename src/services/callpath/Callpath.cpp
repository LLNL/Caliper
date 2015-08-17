///@file  Callpath.cpp
///@brief Callpath provider for caliper records using libunwind

#include "../CaliperService.h"

#include <Caliper.h>

#include <RuntimeConfig.h>
#include <Log.h>

#include <cstring>
#include <string>
#include <type_traits>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define MAX_PATH 40
#define NAMELEN  100

using namespace cali;
using namespace std;

namespace 
{

Attribute callpath_name_attr { Attribute::invalid};
Attribute callpath_addr_attr { Attribute::invalid };

ConfigSet config;

bool      use_name { false };
bool      use_addr { false };

unsigned  skip_frames { 0 };

static const ConfigSet::Entry s_configdata[] = {
    { "use_name", CALI_TYPE_BOOL, "false",
      "Record region names for call path.",
      "Record region names for call path. Incurs higher overhead."
    },
    { "use_address", CALI_TYPE_BOOL, "true",
      "Record region addresses for call path",
      "Record region addresses for call path"
    },
    { "skip_frames", CALI_TYPE_UINT, "10",
      "Skip this number of stack frames",
      "Skip this number of stack frames.\n"
      "Avoids recording stack frames within the caliper library"
    },
    ConfigSet::Terminator
};


void measure_cb(Caliper* c, int scope, WriteRecordFn)
{
    Variant v_addr[MAX_PATH];
    Variant v_name[MAX_PATH];

    char    strbuf[MAX_PATH][NAMELEN];

    // Init unwind context
    unw_context_t unw_ctx;
    unw_cursor_t  unw_cursor;

    unw_getcontext(&unw_ctx);

    if (unw_init_local(&unw_cursor, &unw_ctx) < 0) {
        Log(0).stream() << "callpath::measure_cb: error: unable to init libunwind cursor" << endl;
        return;
    }

    // skip n frames

    size_t n = 0;

    for (n = skip_frames; n > 0 && unw_step(&unw_cursor) > 0; --n)
        ;

    if (n > 0)
        return;

    while (n < MAX_PATH && unw_step(&unw_cursor) > 0) {
        // store path from top to bottom

        if (use_addr) {
            unw_word_t ip;
            unw_get_reg(&unw_cursor, UNW_REG_IP, &ip);

            uint64_t uint = ip;
            v_addr[MAX_PATH-(n+1)] = Variant(CALI_TYPE_ADDR, &uint, sizeof(uint64_t));
        }
        if (use_name) {
            unw_word_t offs;

            if (unw_get_proc_name(&unw_cursor, strbuf[n], NAMELEN, &offs) < 0)
                strncpy(strbuf[n], "UNKNOWN", NAMELEN);

            v_name[MAX_PATH-(n+1)] = Variant(CALI_TYPE_STRING, strbuf[n], strlen(strbuf[n]));
        }

        ++n;
    }

    if (n > 0) {
        if (use_addr)
            c->set_path(callpath_addr_attr, n, v_addr+(MAX_PATH-n));
        if (use_name)
            c->set_path(callpath_name_attr, n, v_name+(MAX_PATH-n));
    }
}

} // namespace 


namespace cali
{

void callpath_service_register(Caliper* c)
{
    config = RuntimeConfig::init("callpath", s_configdata);

    use_name    = config.get("use_name").to_bool();
    use_addr    = config.get("use_address").to_bool();
    skip_frames = config.get("skip_frames").to_uint();

    callpath_addr_attr = 
        c->create_attribute("callpath.address", CALI_TYPE_ADDR,   CALI_ATTR_SKIP_EVENTS);
    callpath_name_attr = 
        c->create_attribute("callpath.regname", CALI_TYPE_STRING, CALI_ATTR_SKIP_EVENTS);    

    c->events().measure.connect(&measure_cb);

    Log(1).stream() << "Registered callpath service" << endl;
}

CaliperService CallpathService = { "callpath", callpath_service_register };

} // namespace cali
