// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file demangle.h
/// Helper function for demangling C++ identifiers

#pragma once

#ifndef UTIL_DEMANGLE_H
#define UTIL_DEMANGLE_H

#include <string>

namespace cali
{
namespace util
{

/// \brief Return demangled name for C++ binary identifier \a name
std::string demangle(const char* name);

} // namespace util
} // namespace cali

#endif
