// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file StringConverter.cpp
/// StringConverter implementation

#include "caliper/common/StringConverter.h"

#include "util/parse_util.h"

#include <algorithm>
#include <cctype>
#include <sstream>

cali_id_t
cali::StringConverter::to_id() const
{
    bool ok = false;
    cali_id_t id = to_uint(&ok);

    return ok ? id : CALI_INV_ID;
}

bool
cali::StringConverter::to_bool(bool* okptr) const
{
    bool ok  = false;
    bool res = false;
    
    // try string
    
    {
        std::string lower;

        std::transform(m_str.begin(), m_str.end(), std::back_inserter(lower),
                       ::tolower);

        if (lower == "true" || lower == "t") {
            ok  = true;
            res = true;
        } else if (lower == "false" || lower == "f") {
            ok  = true;
            res = false;
        }
    }

    // try numeral
    
    if (!ok) {
        try {
            unsigned long i = std::stoul(m_str);
            
            res = (i != 0);
            ok  = true;
        } catch (...) {
            ok  = false;
        }
    }

    if (okptr)
        *okptr = ok;

    return res;
}

int
cali::StringConverter::to_int(bool* okptr) const
{
    bool ok  = false;
    int  res = 0;

    try {
        res = std::stoi(m_str);
        ok  = true;
    } catch (...) {
        ok  = false;
    }

    if (okptr)
        *okptr = ok;

    return res;
}

uint64_t
cali::StringConverter::to_uint(bool* okptr, int base) const
{
    bool ok = false;
    uint64_t res = 0;

    try {
        res = std::stoull(m_str, nullptr, base);
        ok  = true;
    } catch (...) {
        ok  = false;
    }

    if (okptr)
        *okptr = ok;

    return res;    
}

double
cali::StringConverter::to_double(bool* okptr) const
{
    bool   ok  = false;
    double res = 0;

    try {
        res = std::stod(m_str);
        ok  = true;
    } catch (...) {
        ok  = false;
    }

    if (okptr)
        *okptr = ok;

    return res;
}

std::vector<std::string>
cali::StringConverter::to_stringlist(const char* separators, bool* okptr) const
{
    std::vector<std::string> ret;
    char c;

    std::istringstream is(m_str);

    do { 
        std::string str = util::read_word(is, separators);

        if (!str.empty())
            ret.push_back(str);

        c = util::read_char(is);
    } while (is.good() && util::is_one_of(c, separators));

    if (okptr)
        *okptr = true;

    return ret;
}
