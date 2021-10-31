// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file file_util.h
/// Helper functions dealing with files

#pragma once

#ifndef UTIL_FILEUTIL_H
#define UTIL_FILEUTIL_H

#include <string>

namespace cali
{

namespace util
{

/// \brief Create a random filename
///
/// The filename is created from the current date/time, PID,
/// and a random string.
std::string create_filename(const char* ext = ".cali");

} // namespace util

} // namespace cali

#endif
