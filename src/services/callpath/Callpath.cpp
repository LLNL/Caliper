///@file  Callpath.cpp
///@brief Callpath provider for caliper records using libunwind

#include "../CaliperService.h"

#include <Caliper.h>

#include <RuntimeConfig.h>
#include <ContextRecord.h>
#include <Log.h>

#include <string>
#include <type_traits>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#define MAX_PATH 40

using namespace cali;
using namespace std;

namespace 
{

Attribute callpath_attr { Attribute::invalid };

void measure_cb(Caliper* c, int scope, WriteRecordFn)
{
    Variant v_data[MAX_PATH];
    
    // Init unwind context
    unw_context_t unw_ctx;
    unw_cursor_t  unw_cursor;

    unw_getcontext(&unw_ctx);

    if (unw_init_local(&unw_cursor, &unw_ctx) < 0) {
        Log(0).stream() << "callpath::measure_cb: error: unable to init libunwind cursor" << endl;
        return;
    }

    size_t n = 0;

    while (n < MAX_PATH && unw_step(&unw_cursor) > 0) {
        unw_word_t ip;
        unw_get_reg(&unw_cursor, UNW_REG_IP, &ip);

        uint64_t   uint = ip;

        // store path from top to bottom
        v_data[MAX_PATH-(++n)] = Variant(CALI_TYPE_ADDR, &uint, sizeof(uint64_t));
    }

    if (n > 0)
        c->set_path(callpath_attr, n, v_data+(MAX_PATH-n));
}

void callpath_register(Caliper* c)
{
    callpath_attr = 
        c->create_attribute("callpath", CALI_TYPE_ADDR, CALI_ATTR_SKIP_EVENTS);

    c->events().measure.connect(&measure_cb);

    Log(1).stream() << "Registered callpath service" << endl;
}

} // namespace 


namespace cali
{
    CaliperService CallpathService = { "callpath", { ::callpath_register } };
} // namespace cali
