// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
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
write_esc_string(std::ostream& os, const char* str, std::string::size_type size, const char* mask_chars = "\\\"", char esc = '\\')
{
    for (size_t i = 0; i < size; ++i) {
        for (const char* p = mask_chars; *p; ++p)
            if (str[i] == *p) {
                os << esc;
                break;
            }
        
        os << str[i];
    }
    
    return os;
}

inline std::ostream&
write_esc_string(std::ostream& os, const std::string& str, const char* mask_chars = "\\\"", char esc = '\\')
{
    return write_esc_string(os, str.data(), str.size(), mask_chars, esc);
}

std::ostream&
pad_right(std::ostream& os, const std::string& str, std::size_t width);

std::ostream&
pad_left (std::ostream& os, const std::string& str, std::size_t width);

std::string
clamp_string(const std::string& str, std::size_t max_width);

} // namespace util
