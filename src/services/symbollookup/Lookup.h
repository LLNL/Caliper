// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once

#ifndef CALI_SYMBOLLOOKUP_LOOKUP_H
#define CALI_SYMBOLLOOKUP_LOOKUP_H

#include <memory>
#include <string>

namespace cali
{

namespace symbollookup
{

class Lookup {
    struct LookupImpl;
    std::unique_ptr<LookupImpl> mP;

public:

    enum Kind {
        Name = 1, File = 2, Line = 4, Module = 8
    };

    struct Result {
        std::string name;
        std::string file;
        int line;
        std::string module;
        bool success;
    };

    Result lookup(uint64_t address, int what) const;

    Lookup();
    ~Lookup();
};

} // namespace symbollookup

} // namespace cali

#endif
