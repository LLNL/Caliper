// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "parse_util.h"

#include <cctype>

std::string
util::read_word(std::istream& is, const char* separators)
{
    std::string ret;
    char c;

    do {
        c = is.get();
    } while (is.good() && std::isspace(c));

    if (is.good())
        is.unget();

    bool esc = false;

    while (is.good()) {
        c = is.get();

        if (c == '\\') {
            c = is.get();
            if (is.good())
                ret.push_back(c);
            continue;
        } else if (c == '"') {
            esc = !esc;
            continue;
        }

        if (!is.good())
            break;

        if (!esc && (isspace(c) || is_one_of(c, separators))) {
            is.unget();
            break;
        }

        ret.push_back(c);
    }

    return ret;
}

char
util::read_char(std::istream& is)
{
    char c = '\0';

    do {
        c = is.get();
    } while (is.good() && std::isspace(c));

    return c;
}
