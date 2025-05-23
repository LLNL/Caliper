// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Callpath.cpp
// Callpath provider for caliper records using libunwind

#include "../Services.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"

#include <cstring>
#include <string>
#include <sstream>
#include <type_traits>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#ifdef CALIPER_HAVE_LIBDW
#include <elfutils/libdwfl.h>
#include <unistd.h>
#endif

#define MAX_PATH 40
#define NAMELEN 100

using namespace cali;

namespace
{

class Callpath
{
    Attribute callpath_name_attr;
    Attribute callpath_addr_attr;
    Attribute ucursor_attr;

    bool use_name { false };
    bool use_addr { false };
    bool skip_internal { false };

    unsigned skip_frames { 0 };

    Node callpath_root_node;

    uintptr_t caliper_start_addr { 0 };
    uintptr_t caliper_end_addr { 0 };

    void snapshot_cb(Caliper* c, SnapshotView info, SnapshotBuilder& snapshot)
    {
        Variant v_addr[MAX_PATH];
        Variant v_name[MAX_PATH];

        char strbuf[MAX_PATH][NAMELEN];

        // Init unwind context
        unw_cursor_t ucursor;

        Entry e;
        if (ucursor_attr)
            e = info.get(ucursor_attr);
        if (!e.empty()) {
            ucursor = *static_cast<unw_cursor_t*>(e.value().get_ptr());
        } else {
            unw_context_t uctx;
            unw_getcontext(&uctx);

            if (unw_init_local(&ucursor, &uctx) < 0) {
                Log(0).stream() << "callpath: unable to init libunwind cursor\n";
                return;
            }
        }

        // skip n frames

        size_t n = 0;

        for (n = skip_frames; n > 0 && unw_step(&ucursor) > 0; --n)
            ;

        if (n > 0)
            return;

        while (n < MAX_PATH && unw_step(&ucursor) > 0) {

            // skip stack frames inside caliper
            unw_word_t ip;
            unw_get_reg(&ucursor, UNW_REG_IP, &ip);

            if (skip_internal && (ip >= caliper_start_addr && ip < caliper_end_addr))
                continue;

            // store path from top to bottom
            if (use_addr) {
                uint64_t uint              = ip;
                v_addr[MAX_PATH - (n + 1)] = Variant(CALI_TYPE_ADDR, &uint, sizeof(uint64_t));
            }
            if (use_name) {
                unw_word_t offs;

                if (unw_get_proc_name(&ucursor, strbuf[n], NAMELEN, &offs) < 0)
                    strncpy(strbuf[n], "UNKNOWN", NAMELEN);

                v_name[MAX_PATH - (n + 1)] = Variant(CALI_TYPE_STRING, strbuf[n], strlen(strbuf[n]));
            }

            ++n;
        }

        if (n > 0) {
            if (use_addr)
                snapshot.append(
                    Entry(c->make_tree_entry(callpath_addr_attr, n, v_addr + (MAX_PATH - n), &callpath_root_node))
                );
            if (use_name)
                snapshot.append(
                    Entry(c->make_tree_entry(callpath_name_attr, n, v_name + (MAX_PATH - n), &callpath_root_node))
                );
        }
    }

    void get_caliper_module_addresses()
    {
#ifdef CALIPER_HAVE_LIBDW
        // initialize dwarf
        char*          debuginfopath = nullptr;
        Dwfl_Callbacks callbacks;

        callbacks.find_elf       = dwfl_linux_proc_find_elf;
        callbacks.find_debuginfo = dwfl_standard_find_debuginfo;
        callbacks.debuginfo_path = &debuginfopath;

        Dwfl* dwfl = dwfl_begin(&callbacks);

        dwfl_linux_proc_report(dwfl, getpid());
        dwfl_report_end(dwfl, nullptr, nullptr);

        // Init unwind context
        unw_context_t uctx;
        unw_cursor_t  ucursor;

        unw_getcontext(&uctx);

        if (unw_init_local(&ucursor, &uctx) < 0) {
            Log(0).stream() << "callpath: unable to init libunwind\n";
            return;
        }

        // Get current (caliper) module
        unw_word_t ip;
        unw_get_reg(&ucursor, UNW_REG_IP, &ip);

        Dwfl_Module* mod   = dwfl_addrmodule(dwfl, ip);
        Dwarf_Addr   start = 0;
        Dwarf_Addr   end   = 0;

        dwfl_module_info(mod, nullptr, &start, &end, nullptr, nullptr, nullptr, nullptr);

        caliper_start_addr = start;
        caliper_end_addr   = end;

        if (Log::verbosity() >= 2) {
            std::ostringstream os;
            os << std::hex << caliper_start_addr << ":" << caliper_end_addr;

            Log(2).stream() << "callpath: skipping internal caliper frames (" << os.str() << ")" << std::endl;
        }

        dwfl_end(dwfl);
#endif
    }

    void post_init_evt(Caliper* c, Channel*) { ucursor_attr = c->get_attribute("cali.unw_cursor"); }

    Callpath(Caliper* c, Channel* chn) : callpath_root_node(CALI_INV_ID, CALI_INV_ID, Variant())
    {
        ConfigSet config = services::init_config_from_spec(chn->config(), s_spec);

        use_name      = config.get("use_name").to_bool();
        use_addr      = config.get("use_address").to_bool();
        skip_frames   = config.get("skip_frames").to_uint();
        skip_internal = config.get("skip_internal").to_bool();

        Attribute symbol_class_attr = c->get_attribute("class.symboladdress");
        Variant   v_true(true);

        callpath_addr_attr = c->create_attribute(
            "callpath.address",
            CALI_TYPE_ADDR,
            CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS,
            1,
            &symbol_class_attr,
            &v_true
        );
        callpath_name_attr =
            c->create_attribute("callpath.regname", CALI_TYPE_STRING, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_SKIP_EVENTS);

#ifdef CALIPER_HAVE_LIBDW
        if (skip_internal)
            get_caliper_module_addresses();
#else
        skip_internal = false;
#endif
    }

public:

    static const char* s_spec;

    static void callpath_service_register(Caliper* c, Channel* chn)
    {
        Callpath* instance = new Callpath(c, chn);

        chn->events().post_init_evt.connect([instance](Caliper* c, Channel* chn) { instance->post_init_evt(c, chn); });
        chn->events().snapshot.connect(
            [instance](Caliper* c, SnapshotView info, SnapshotBuilder& snapshot) {
                instance->snapshot_cb(c, info, snapshot);
            }
        );
        chn->events().finish_evt.connect([instance](Caliper* c, Channel* chn) { delete instance; });

        Log(1).stream() << chn->name() << ": Registered callpath service" << std::endl;
    }

}; // class Callpath

const char* Callpath::s_spec = R"json(
{
 "name"        : "callpath",
 "description" : "Record call stack at each snapshot",
 "config"      :
 [
  {
   "name": "use_name",
   "type": "bool",
   "description": "Record function names",
   "value": "false"
  },{
   "name": "use_address",
   "type": "bool",
   "description": "Record function addresses",
   "value": "true"
  },{
   "name": "skip_frames",
   "type": "uint",
   "description": "Skip this number of stack frames",
   "value": "0"
  },{
   "name": "skip_internal",
   "type": "bool",
   "description": "Skip internal (inside Caliper library) stack frames",
   "value": "true"
  }
 ]
}
)json";

} // namespace

namespace cali
{

CaliperService callpath_service = { ::Callpath::s_spec, ::Callpath::callpath_service_register };

} // namespace cali
