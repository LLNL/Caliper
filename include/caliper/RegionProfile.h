// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

/// \file RuntimeProfile.h
/// RuntimeProfile class

#pragma once

#include "ChannelController.h"

#include <map>
#include <memory>
#include <tuple>

namespace cali
{

class RegionProfile : public ChannelController
{
    struct RegionProfileImpl;
    std::shared_ptr<RegionProfileImpl> mP;
    
public:

    RegionProfile();

    typedef std::tuple< std::map<std::string, double>, double, double > region_profile_t;

    region_profile_t
    exclusive_region_times(const std::string& region_type = "");

    region_profile_t
    inclusive_region_times(const std::string& region_type = "");

    void
    clear();
};

}
