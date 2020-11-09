// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Callpath.cpp
// Callpath provider for caliper records using libunwind

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RuntimeConfig.h"

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
#define NAMELEN  100

using namespace cali;
using namespace std;

namespace
{

class Callpath
{
    static const ConfigSet::Entry s_configdata[];

    Attribute callpath_name_attr { Attribute::invalid};
    Attribute callpath_addr_attr { Attribute::invalid };

    bool      use_name { false };
    bool      use_addr { false };
    bool      skip_internal { false };

    unsigned  skip_frames { 0 };

    Node      callpath_root_node;

    uintptr_t caliper_start_addr { 0 };
    uintptr_t caliper_end_addr   { 0 };

    void snapshot_cb(Caliper* c, Channel* chn, int scope, const SnapshotRecord*, SnapshotRecord* snapshot) {
        Variant v_addr[MAX_PATH];
        Variant v_name[MAX_PATH];

        char    strbuf[MAX_PATH][NAMELEN];

        // Init unwind context
        unw_context_t unw_ctx;
        unw_cursor_t  unw_cursor;

#ifdef __aarch64__
        unw_getcontext(unw_ctx);
#else
        unw_getcontext(&unw_ctx);
#endif

        if (unw_init_local(&unw_cursor, &unw_ctx) < 0) {
            Log(0).stream() << "callpath: unable to init libunwind cursor" << endl;
            return;
        }

        // skip n frames

        size_t n = 0;

        for (n = skip_frames; n > 0 && unw_step(&unw_cursor) > 0; --n)
            ;

        if (n > 0)
            return;

        while (n < MAX_PATH && unw_step(&unw_cursor) > 0) {

            // skip stack frames inside caliper
            unw_word_t ip;
            unw_get_reg(&unw_cursor, UNW_REG_IP, &ip);

            if (skip_internal && (ip >= caliper_start_addr && ip < caliper_end_addr))
                continue;

            // store path from top to bottom
            if (use_addr) {
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
                snapshot->append(
                    c->make_tree_entry(callpath_addr_attr, n, v_addr+(MAX_PATH-n),
                                       &callpath_root_node));
            if (use_name)
                snapshot->append(
                    c->make_tree_entry(callpath_name_attr, n, v_name+(MAX_PATH-n),
                                       &callpath_root_node));
        }
    }

    void get_caliper_module_addresses() {
#ifdef CALIPER_HAVE_LIBDW
        // initialize dwarf
        char* debuginfopath = nullptr;
        Dwfl_Callbacks callbacks;

        callbacks.find_elf = dwfl_linux_proc_find_elf;
        callbacks.find_debuginfo = dwfl_standard_find_debuginfo;
        callbacks.debuginfo_path = &debuginfopath;

        Dwfl* dwfl = dwfl_begin(&callbacks);

        dwfl_linux_proc_report(dwfl, getpid());
        dwfl_report_end(dwfl, nullptr, nullptr);

        // Init unwind context
        unw_context_t unw_ctx;
        unw_cursor_t  unw_cursor;

        unw_getcontext(&unw_ctx);

        if (unw_init_local(&unw_cursor, &unw_ctx) < 0) {
            Log(0).stream() << "callpath::measure_cb: error: unable to init libunwind" << endl;
            return;
        }

        // Get current (caliper) module
        unw_word_t ip;
        unw_get_reg(&unw_cursor, UNW_REG_IP, &ip);

        Dwfl_Module* mod = dwfl_addrmodule(dwfl, ip);
        Dwarf_Addr start = 0;
        Dwarf_Addr end = 0;

        dwfl_module_info(mod, nullptr, &start, &end, nullptr, nullptr, nullptr, nullptr);

        caliper_start_addr = start;
        caliper_end_addr = end;

        if (Log::verbosity() >= 2) {
            std::ostringstream os;
            os << std::hex << caliper_start_addr << ":" << caliper_end_addr;

            Log(2).stream() << "callpath: skipping internal caliper frames ("
                            << os.str() << ")"
                            << std::endl;
        }

        dwfl_end(dwfl);
#endif
    }

    Callpath(Caliper* c, Channel* chn)
        : callpath_root_node(CALI_INV_ID, CALI_INV_ID, Variant())
        {
            ConfigSet config =
                chn->config().init("callpath", s_configdata);

            use_name      = config.get("use_name").to_bool();
            use_addr      = config.get("use_address").to_bool();
            skip_frames   = config.get("skip_frames").to_uint();
            skip_internal = config.get("skip_internal").to_bool();

            Attribute symbol_class_attr = c->get_attribute("class.symboladdress");
            Variant v_true(true);

            callpath_addr_attr =
                c->create_attribute("callpath.address", CALI_TYPE_ADDR,
                                    CALI_ATTR_SCOPE_THREAD |
                                    CALI_ATTR_SKIP_EVENTS  |
                                    CALI_ATTR_NOMERGE,
                                    1, &symbol_class_attr, &v_true);
            callpath_name_attr =
                c->create_attribute("callpath.regname", CALI_TYPE_STRING,
                                    CALI_ATTR_SCOPE_THREAD |
                                    CALI_ATTR_SKIP_EVENTS  |
                                    CALI_ATTR_NOMERGE);

#ifdef CALIPER_HAVE_LIBDW
            if (skip_internal)
                get_caliper_module_addresses();
#else
            skip_internal = false;
#endif
        }

public:

    static void callpath_service_register(Caliper* c, Channel* chn) {
        Callpath* instance = new Callpath(c, chn);

        chn->events().snapshot.connect(
            [instance](Caliper* c, Channel* chn, int scope, const SnapshotRecord* info, SnapshotRecord* snapshot){
                instance->snapshot_cb(c, chn, scope, info, snapshot);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                delete instance;
            });

        Log(1).stream() << chn->name() << ": Registered callpath service" << std::endl;
    }

}; // class Callpath

const ConfigSet::Entry Callpath::s_configdata[] = {
    { "use_name", CALI_TYPE_BOOL, "false",
      "Record region names for call path.",
      "Record region names for call path. Incurs higher overhead."
    },
    { "use_address", CALI_TYPE_BOOL, "true",
      "Record region addresses for call path",
      "Record region addresses for call path"
    },
    { "skip_frames", CALI_TYPE_UINT, "0",
      "Skip this number of stack frames",
      "Skip this number of stack frames.\n"
      "Avoids recording stack frames within the caliper library"
    },
    { "skip_internal", CALI_TYPE_BOOL, "true",
      "Skip caliper-internal stack frames",
      "Skip caliper-internal stack frames. Requires libdw support.\n"
    },
    ConfigSet::Terminator
};

} // namespace [anonymous]


namespace cali
{

CaliperService callpath_service = { "callpath", ::Callpath::callpath_service_register };

} // namespace cali
