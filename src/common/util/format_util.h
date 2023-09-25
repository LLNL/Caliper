// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file format_util.h
/// Helper functions for formatting strings

#pragma once

#include <iostream>
#include <string>

namespace util
{

/// \brief Write string \a str to \a os,
///   escaping all characters in \a mask_chars with \a esc.
inline std::ostream&
write_json_esc_string(std::ostream& os, const char* str, std::string::size_type size)
{
    for (size_t i = 0; i < size; ++i) {
        const char c = str[i];

        if (c == '\n') // handle newline in string
            os << "\\n";
        if (c < 0x20)  // skip control characters
            continue;
        if (c == '\\' || c == '\"')
            os << '\\';

        os << c;
    }

    return os;
}

/// \brief Write string \a str to \a os,
///   escaping all characters in \a mask_chars with \a esc.
inline std::ostream&
write_cali_esc_string(std::ostream& os, const char* str, std::string::size_type size)
{
    for (size_t i = 0; i < size; ++i) {
        const char c = str[i];

        if (c == '\n') // handle newline in string
            os << "\\n";
        if (c < 0x20)  // skip control characters
            continue;
        if (c == '\\' || c == ',' || c == '=')
            os << '\\';

        os << c;
    }

    return os;
}

inline std::ostream&
write_json_esc_string(std::ostream& os, const std::string& str)
{
    return write_json_esc_string(os, str.data(), str.size());
}

inline std::ostream&
write_cali_esc_string(std::ostream& os, const std::string& str)
{
    return write_cali_esc_string(os, str.data(), str.size());
}

std::ostream&
pad_right(std::ostream& os, const std::string& str, std::size_t width);

std::ostream&
pad_left (std::ostream& os, const std::string& str, std::size_t width);

std::string
clamp_string(const std::string& str, std::size_t max_width);

} // namespace util
