// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
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
            { "CALI_TIMER_SNAPSHOT_DURATION",  "true"  },
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
    FlatExclusiveRegionProfile rp(c, "sum#time.duration", region_type.c_str());

    if (channel())
        c.flush(channel(), nullptr, rp);
    else
        Log(1).stream() << "RegionProfile::exclusive_region_times(): channel is not enabled"
                        << std::endl;

    return rp.result();
}


RegionProfile::region_profile_t
RegionProfile::inclusive_region_times(const std::string& region_type)
{
    Caliper c;
    FlatInclusiveRegionProfile rp(c, "sum#time.duration", region_type.c_str());

    if (channel())
        c.flush(channel(), nullptr, rp);
    else
        Log(1).stream() << "RegionProfile::inclusive_region_times(): channel is not enabled"
                        << std::endl;

    return rp.result();
}

void
RegionProfile::clear()
{
    Channel* chn = channel();
    
    if (chn)
        Caliper::instance().clear(chn);
}
