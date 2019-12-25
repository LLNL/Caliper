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

std::string
util::read_nested_text(std::istream& is, char start_char, char end_char)
{
    std::string ret;
    bool esc = false;
    int  depth = 1;

    while (is.good()) {
        char c = is.get();

        if (c == '\\') {
            c = is.get();
            if (is.good()) {
                ret.push_back('\\');
                ret.push_back(c);
            }
            continue;
        } else if (c == '"') {
            esc = !esc;
        } else if (!esc && c == start_char) {
            ++depth;
        } else if (!esc && c == end_char) {
            --depth;
        }

        if (!is.good())
            break;

        if (depth == 0) {
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
