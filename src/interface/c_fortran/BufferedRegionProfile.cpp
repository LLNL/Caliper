// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "BufferedRegionProfile.h"

using namespace cali;

struct BufferedRegionProfile::BufferedRegionProfileImpl
{
    RegionProfile::region_profile_t result;
};

BufferedRegionProfile::BufferedRegionProfile()
    : RegionProfile(),
      mP { new BufferedRegionProfileImpl }
{ }

BufferedRegionProfile::~BufferedRegionProfile()
{ }

void BufferedRegionProfile::start()
{
    RegionProfile::start();
}

void BufferedRegionProfile::stop()
{
    RegionProfile::stop();
}

void BufferedRegionProfile::clear()
{
    std::get<0>(mP->result).clear();
    std::get<1>(mP->result) = 0.0;
    std::get<2>(mP->result) = 0.0;

    RegionProfile::clear();
}

void BufferedRegionProfile::fetch_exclusive_region_times(const char* region_type)
{
    mP->result = exclusive_region_times(region_type);
}

void BufferedRegionProfile::fetch_inclusive_region_times(const char* region_type)
{
    mP->result = inclusive_region_times(region_type);
}

double BufferedRegionProfile::total_profiling_time() const
{
    return std::get<2>(mP->result);
}

double BufferedRegionProfile::total_region_time() const
{
    return std::get<1>(mP->result);
}

double BufferedRegionProfile::region_time(const char* region) const
{
    auto it = std::get<0>(mP->result).find(region);
    return (it == std::get<0>(mP->result).end() ? 0.0 : it->second);
}
