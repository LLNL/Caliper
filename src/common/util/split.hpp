// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// @file  split.hpp
/// @brief String split operation

#ifndef UTIL_SPLIT_HPP
#define UTIL_SPLIT_HPP

namespace util
{

template<typename String, typename Char, typename OutputIterator>
void split(String input, Char sep, OutputIterator out)
{
    String str;

    for ( Char c : input ) {
        if (c == sep) {
            *out++ = str;
            str.clear();
        } else {
            str.push_back(c);
        }
    }

    if (!str.empty())
        *out++ = str;
}

template<typename String, typename Char, typename OutputIterator>
void tokenize(String input, const Char* tokens, OutputIterator out)
{
    String str;

    for ( Char c : input ) {
        bool is_token_char = false;
        for ( const Char* tc = tokens; *tc; ++tc )
            if (c == *tc) {
                is_token_char = true; break;
            }

        if (is_token_char) {
            if (!str.empty())
                *out++ = str;
            str.clear();

            str.push_back(c);
            *out++ = str;
            str.clear();
        } else {
            str.push_back(c);
        }
    }

    if (!str.empty())
        *out++ = str;
}

} // namespace util

#endif
