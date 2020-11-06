// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file StringConverter.h
/// A class to convert strings into various other data types.
/// This is primarily a convenience class to transparently replace
/// cali::Variant's former string conversion capabilities.

#pragma once

#include "cali_types.h"

#include <map>
#include <string>
#include <vector>

namespace cali
{

class StringConverter {
    std::string m_str;

public:

    StringConverter()
    : m_str()
    { }

    StringConverter(const std::string& str)
        : m_str(str)
    { }

    cali_id_t     to_id() const;

    bool          to_bool(bool* okptr = nullptr)   const;

    int           to_int(bool* okptr = nullptr)    const;
    int64_t       to_int64(bool* okptr = nullptr)  const;
    uint64_t      to_uint(bool* okptr = nullptr, int base = 10) const;

    double        to_double(bool* okptr = nullptr) const;

    std::string   to_string(bool* okptr = nullptr) const {
        if (okptr)
            *okptr = true;
        return m_str;
    }

    std::vector<std::string>
    to_stringlist(const char* separators = ",", bool* okptr = nullptr) const;

    std::vector<StringConverter>
    rec_list(bool* okptr = nullptr) const;

    std::map<std::string, StringConverter>
    rec_dict(bool* okptr = nullptr) const;
};

} // namespace cali
