// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#pragma once
#ifndef CALI_CONTROLLERS_UTIL_H
#define CALI_CONTROLLERS_UTIL_H

#include "caliper/ConfigManager.h"

#include <string>

namespace cali
{

namespace util
{

std::string build_tree_format_spec(config_map_t&, const cali::ConfigManager::Options&, const char* initial = "");

}

}

#endif // CALI_CONTROLLERS_UTIL_H
