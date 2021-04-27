// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "Lookup.h"

#include "caliper/common/Log.h"

#include "../../common/util/demangle.h"

#include <elfutils/libdwfl.h>
#include <unistd.h>

#include <cstring>

using namespace cali;
using namespace symbollookup;

namespace
{

Dwfl_Callbacks* get_dwfl_callbacks()
{
    static char* debuginfopath = nullptr;

    static bool initialized = false;
    static Dwfl_Callbacks callbacks;

    if (!initialized) {
        callbacks.find_elf = dwfl_linux_proc_find_elf;
        callbacks.find_debuginfo = dwfl_standard_find_debuginfo;
        callbacks.debuginfo_path = &debuginfopath;
        initialized = true;
    }

    return &callbacks;
}

} // namespace [anonymous]


struct Lookup::LookupImpl
{
    Dwfl* dwfl { nullptr };

    Lookup::Result
    lookup(uintptr_t address, int what) {
        Result result { "UNKNOWN", "UNKNOWN", 0, "UNKNOWN", false };

        if (!dwfl)
            return result;

        Dwfl_Module *mod = dwfl_addrmodule (dwfl, address);

        if (!mod)
            return result;

        result.success = true;

        if (what & Kind::Name)
            result.name = util::demangle(dwfl_module_addrname(mod, address));

        if (what & Kind::File || what & Kind::Line) {
            Dwfl_Line *line = dwfl_module_getsrc(mod, address);

            if (line) {
                int lineno, linecol;
                const char* src = dwfl_lineinfo(line, &address, &lineno, &linecol, nullptr, nullptr);

                if (src) {
                    result.file = src;
                    result.line = lineno;
                }
            }
        }

        if (what & Kind::Module) {
            const char* name = dwfl_module_info(mod, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

            if (name)
                result.module = name;
        }

        return result;
    }

    LookupImpl() {
        Log(2).stream() << "symbollookup: Loading debug info" << std::endl;

        dwfl = dwfl_begin(get_dwfl_callbacks());

        if (dwfl_linux_proc_report(dwfl, getpid()) != 0) {
            Log(0).stream() << "symbollookup: dwfl_linux_proc_report() error: "
                            << dwfl_errmsg(dwfl_errno())
                            << std::endl;
            dwfl_end(dwfl);
            dwfl = nullptr;
            return;
        }

        if (dwfl_report_end(dwfl, nullptr, nullptr) != 0) {
            Log(0).stream() << "symbollookup: dwfl_report_end() error: "
                            << dwfl_errmsg(dwfl_errno())
                            << std::endl;
            dwfl_end(dwfl);
            dwfl = nullptr;
        }
    }

    ~LookupImpl() {
        dwfl_end(dwfl);
    }
};


Lookup::Result
Lookup::lookup(uint64_t address, int what) const
{
    return mP->lookup(static_cast<uintptr_t>(address), what);
}

Lookup::Lookup()
    : mP(new LookupImpl)
{
}

Lookup::~Lookup()
{
}
