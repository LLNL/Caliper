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

int64_t
cali::StringConverter::to_int64(bool* okptr) const
{
    bool    ok  = false;
    int64_t res = 0;

    try {
        res = std::stoll(m_str);
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

std::vector<cali::StringConverter>
cali::StringConverter::rec_list(bool* okptr) const
{
    std::vector<StringConverter> ret;
    bool error = false;
    char c;

    std::istringstream is(m_str);

    int d = 0;
    c = util::read_char(is);

    if (c == '[')
        ++d;
    else if (is.good())
        is.unget();

    do {
        c = util::read_char(is);
        std::string str;

        if (c == '{') {
            str = "{";
            str.append(util::read_nested_text(is, '{', '}'));

            c = util::read_char(is);
            if (c != '}') {
                error = true;
                break;
            }

            str.append("}");
        } else if (c == '[') {
            str = "[";
            str.append(util::read_nested_text(is, '[', ']'));

            c = util::read_char(is);
            if (c != ']') {
                error = true;
                break;
            }
            str.append("]");
        } else {
            is.unget();
            str = util::read_word(is, ",]");
        }

        if (!str.empty())
            ret.push_back(StringConverter(str));

        c = util::read_char(is);
    } while (is.good() && util::is_one_of(c, ","));

    if (d > 0) {
        if (c != ']')
            error = true;
    } else
        is.unget();

    if (okptr)
        *okptr = !error;

    return ret;
}

std::map<std::string, cali::StringConverter>
cali::StringConverter::rec_dict(bool *okptr) const
{
    std::map<std::string, StringConverter> ret;
    bool error = false;
    char c;

    std::istringstream is(m_str);

    int d = 0;
    c = util::read_char(is);

    if (c == '{')
        ++d;
    else
        is.unget();

    do {
        std::string key = util::read_word(is, ":");
        c = util::read_char(is);

        if (c != ':') {
            if (okptr)
                *okptr = false;
            return ret;
        }

        std::string val;
        c = util::read_char(is);

        if (c == '{') {
            val = "{";
            val.append(util::read_nested_text(is, '{', '}'));

            c = util::read_char(is);
            if (c != '}') {
                error = true;
                break;
            }
            val.append("}");
        } else if (c == '[') {
            val = "[";
            val.append(util::read_nested_text(is, '[', ']'));

            c = util::read_char(is);
            if (c != ']') {
                error = true;
                break;
            }
            val.append("]");
        } else {
            is.unget();
            val = util::read_word(is, ",}");
        }

        if (!key.empty())
            ret[key] = StringConverter(val);

        c = util::read_char(is);
    } while (is.good() && c == ',');

    if (d > 0) {
        if (c != '}')
            error = true;
    } else
        is.unget();

    if (okptr)
        *okptr = !error;

    return ret;
}
