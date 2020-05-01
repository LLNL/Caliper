// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "BufferedRegionProfile.h"

using namespace cali;

struct BufferedRegionProfile::BufferedRegionProfileImpl
{
    std::map<std::string, double> reg_times;
    double tot_reg_time;
    double tot_time;
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
    mP->reg_times.clear();
    mP->tot_reg_time = 0.0;
    mP->tot_time = 0.0;
    RegionProfile::clear();
}

void BufferedRegionProfile::fetch_exclusive_region_times(const char* region_type)
{
    std::tie(mP->reg_times, mP->tot_reg_time, mP->tot_time)
        = exclusive_region_times(region_type);
}

void BufferedRegionProfile::fetch_inclusive_region_times(const char* region_type)
{
    std::tie(mP->reg_times, mP->tot_reg_time, mP->tot_time)
        = inclusive_region_times(region_type);
}

double BufferedRegionProfile::total_profiling_time() const
{
    return mP->tot_time;
}

double BufferedRegionProfile::total_region_time() const
{
    return mP->tot_reg_time;
}

double BufferedRegionProfile::region_time(const char* region) const
{
    auto it = mP->reg_times.find(region);
    return (it == (mP->reg_times).end() ? 0.0 : it->second);
}
