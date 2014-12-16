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

    *out++ = str;
}

} // namespace util

#endif
