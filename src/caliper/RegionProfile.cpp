// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/RegionProfile.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/FlatExclusiveRegionProfile.h"
#include "caliper/reader/FlatInclusiveRegionProfile.h"

#include "caliper/common/Log.h"

#include "caliper/Caliper.h"

#include <algorithm>

using namespace cali;


struct RegionProfile::RegionProfileImpl
{

};


RegionProfile::RegionProfile()
    : ChannelController("region-profile", 0, {
            { "CALI_SERVICES_ENABLE", "aggregate,event,timestamp" },
            { "CALI_CHANNEL_FLUSH_ON_EXIT",    "false" },
            { "CALI_CHANNEL_CONFIG_CHECK",     "false" },
            { "CALI_EVENT_ENABLE_SNAPSHOT_INFO", "false" },
            { "CALI_TIMER_INCLUSIVE_DURATION", "false" },
            { "CALI_TIMER_UNIT",               "sec"   }
        })
    , mP { new RegionProfileImpl }
{ }

RegionProfile::~RegionProfile()
{ }

RegionProfile::region_profile_t
RegionProfile::exclusive_region_times(const std::string& region_type)
{
    Caliper c;
    Channel chn = channel();
    FlatExclusiveRegionProfile rp(c, "sum#time.duration.ns", region_type.c_str());

    if (chn)
        c.flush(&chn, SnapshotView(), rp);
    else
        Log(1).stream() << "RegionProfile::exclusive_region_times(): channel is not enabled"
                        << std::endl;

    auto res = rp.result();

    // convert from nanosec to sec
    std::get<1>(res) *= 1e-9;
    std::get<2>(res) *= 1e-9;
    for (auto &p : std::get<0>(res))
        p.second *= 1e-9;

    return res;
}


RegionProfile::region_profile_t
RegionProfile::inclusive_region_times(const std::string& region_type)
{
    Caliper c;
    Channel chn = channel();
    FlatInclusiveRegionProfile rp(c, "sum#time.duration.ns", region_type.c_str());

    if (chn)
        c.flush(&chn, SnapshotView(), rp);
    else
        Log(1).stream() << "RegionProfile::inclusive_region_times(): channel is not enabled"
                        << std::endl;

    auto res = rp.result();

    std::get<1>(res) *= 1e-9;
    std::get<2>(res) *= 1e-9;
    for (auto &p : std::get<0>(res))
        p.second *= 1e-9;

    return res;
}

void
RegionProfile::clear()
{
    Channel chn = channel();

    if (chn)
        Caliper::instance().clear(&chn);
}
