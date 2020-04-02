// Copyright (c) 2020, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "Lookup.h"

#include <Symtab.h>
#include <LineInformation.h>
#include <Function.h>
#include <AddrLookup.h>

#include "caliper/common/Log.h"

#include <mutex>
#include <vector>

using namespace Dyninst;
using namespace SymtabAPI;

using namespace cali;
using namespace symbollookup;

struct Lookup::LookupImpl
{
    AddressLookup* lookup;
    std::mutex     lookup_mutex;
};

Lookup::Result
Lookup::lookup(uint64_t address, int what) const
{
    Result result { "UNKNOWN", "UNKNOWN", 0, "UNKNOWN", false };

    bool lookup_name = (what & Kind::Name);
    bool lookup_file = (what & Kind::File);
    bool lookup_line = (what & Kind::Line);
    bool lookup_mod  = (what & Kind::Module);

    std::lock_guard<std::mutex>
        g(mP->lookup_mutex);

    if (!mP->lookup)
        return result;

    Symtab* symtab;
    Offset  offset;

    bool ret_line = false;
    bool ret_func = false;

    bool ret = mP->lookup->getOffset(address, symtab, offset);

    if (ret && (lookup_file || lookup_line)) {
        std::vector<Statement*> statements;
        ret_line = symtab->getSourceLines(statements, offset);

        if (ret_line && statements.size() > 0) {
            result.line = static_cast<int>(statements.front()->getLine());
            result.file = statements.front()->getFile();
            result.success = true;
        }
    }

    if (ret && lookup_name) {
        SymtabAPI::Function* function = 0;
        ret_func = symtab->getContainingFunction(offset, function);

        if (ret_func && function) {
            auto it = function->pretty_names_begin();

            if (it != function->pretty_names_end()) {
                result.name = *it;
                result.success = true;
            }
        }
    }

    if (ret && lookup_mod) {
        result.module  = symtab->name();
        result.success = true;
    }

    return result;
}


Lookup::Lookup()
    : mP { new LookupImpl }
{
    mP->lookup = AddressLookup::createAddressLookup();

    if (!mP->lookup)
        Log(0).stream() << "Symbollookup: Could not create address lookup object"
                        << std::endl;

    mP->lookup->refresh();
}

Lookup::~Lookup()
{
    mP.reset();
}