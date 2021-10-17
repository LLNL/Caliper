// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "demangle.h"

#ifndef _WIN32
#include <cxxabi.h>
#endif

#include <cstring>

namespace util
{

std::string demangle(const char* name)
{
#ifdef _WIN32
    // not sure how to demangle on Windows, make it a no-op for now.
    return name;
#else
    std::string result;

    if (!name)
        return result;

    char* demangled = nullptr;
    int status = -1;

    if (name[0] == '_' && name[1] == 'Z')
        demangled = abi::__cxa_demangle(name, nullptr, 0, &status);

    if (status == 0)
        result = demangled;
    else
        result = name;

    free(demangled);

    return result;
#endif
}

} // namespace util
