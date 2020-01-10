// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file parse_util.h
/// Helper functions to parse config strings etc.

#pragma once

#include <iostream>
#include <string>

namespace util
{

/// \brief Check if \a c is in \a separators
inline bool
is_one_of(char c, const char* characters)
{
    for (const char* ptr = characters; *ptr != '\0'; ++ptr)
        if (*ptr == c)
            return true;

    return false;
}

/// \brief Read from stream \a is until whitespace or one of the characters in
///   \a separators is found.
std::string
read_word(std::istream& is, const char* separators = ",");

/// \brief Reads text within (start_char, end_char) region, skipping
///   over any such regions nested within.
std::string
read_nested_text(std::istream& is, char start_char, char end_char);

/// \brief Read character from stream \a is, skipping whitespace.
char
read_char(std::istream& is);

}
