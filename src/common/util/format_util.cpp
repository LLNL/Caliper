// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "format_util.h"

namespace
{

const char whitespace[80+1] =
    "                                        "
    "                                        ";

}

std::ostream&
util::pad_right(std::ostream& os, const std::string& str, std::size_t width)
{
    os << str;

    if (str.size() > width)
        os << ' ';
    else {
        std::size_t s = 1 + width - str.size();

        for ( ; s > 80; s -= 80)
            os << ::whitespace;

        os << ::whitespace+(80-s);
    }
    
    return os;
}

std::ostream&
util::pad_left (std::ostream& os, const std::string& str, std::size_t width)
{
    if (str.size() < width) {
        std::size_t s = width - str.size();

        for ( ; s > 80; s -= 80)
            os << ::whitespace;

        os << ::whitespace+(80-s);
    }

    os << str << ' ';
    
    return os;
}

std::string
util::clamp_string(const std::string& str, std::size_t max_width)
{
    if (str.length() <= max_width)
        return str;
    if (max_width < 4)
        return str.substr(0, max_width);

    std::string ret;
    ret.reserve(1 + max_width);
    
    ret.append(str, 0, max_width/2-1);
    ret.append("~~");
    ret.append(str, str.length()-(max_width/2-1), std::string::npos);

    return ret;
}
