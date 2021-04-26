// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file demangle.h
/// Helper function for demangling C++ identifiers

#pragma once

#ifndef UTIL_DEMANGLE_H
#define UTIL_DEMANGLE_H

#include <string>

namespace util
{

/// \brief Return demangled name for C++ binary identifier \a name
std::string demangle(const char* name);

}

#endif
