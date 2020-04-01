// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "Lookup.h"

#include "caliper/common/Log.h"

#include <cxxabi.h>
#include <dlfcn.h>

#include <cstring>

using namespace cali;
using namespace symbollookup;

struct Lookup::LookupImpl
{
};

Lookup::Result
Lookup::lookup(uint64_t address, int what) const
{
    Result result { "UNKNOWN", "UNKNOWN", 0, "UNKNOWN", false };

    Dl_info dlinfo;
    if (dladdr(reinterpret_cast<void*>(address), &dlinfo) && dlinfo.dli_sname) {
        if (what & Kind::Name) {
            char* demangled = nullptr;
            int status = -1;

            if (dlinfo.dli_sname[0] == '_')
                demangled = abi::__cxa_demangle(dlinfo.dli_sname, nullptr, 0, &status);

            if (status == 0)
                result.name = demangled;
            else
                result.name = dlinfo.dli_sname;

            free(demangled);
        }

        if ((what & Kind::Module) && dlinfo.dli_fname)
            result.module = dlinfo.dli_fname;

        result.success = true;
    }

    return result;
}

Lookup::Lookup()
{ }

Lookup::~Lookup()
{ }