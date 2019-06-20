// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "caliper/reader/FlatExclusiveRegionProfile.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include <algorithm>
#include <cstring>

using namespace cali;

struct FlatExclusiveRegionProfile::FlatExclusiveRegionProfileImpl
{        
    double    total     = 0.0;
    double    total_reg = 0.0;
    std::map<std::string, double> reg_profile;
    
    Attribute metric_attr;
    Attribute region_attr;

    void process_record(CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec) {
        cali_id_t metric_attr_id = metric_attr.id();

        auto metric_entry_it = std::find_if(rec.begin(), rec.end(), [metric_attr_id](const Entry& e){
                return e.attribute() == metric_attr_id;
            });

        if (metric_entry_it == rec.end())
            return;

        double val = metric_entry_it->value().to_double();
        total += val;

        auto region_entry_it = rec.end();

        if (region_attr == Attribute::invalid)
            region_entry_it = std::find_if(rec.begin(), rec.end(), [&db](const Entry& e){
                    return db.get_attribute(e.attribute()).is_nested();
                });
        else {
            cali_id_t attr_id = region_attr.id();
            region_entry_it = std::find_if(rec.begin(), rec.end(), [attr_id](const Entry& e){
                    return e.attribute() == attr_id;
                });
        }
        
        if (region_entry_it != rec.end()) {    
            total_reg += val;
            reg_profile[region_entry_it->value().to_string()] += val;
        }
    }
};


FlatExclusiveRegionProfile::FlatExclusiveRegionProfile(CaliperMetadataAccessInterface& db,
                                                       const char* metric_attr_name,
                                                       const char* region_attr_name)
    : mP { new FlatExclusiveRegionProfileImpl }
{
    mP->metric_attr = db.get_attribute(metric_attr_name);

    if (region_attr_name && strlen(region_attr_name) > 0)
        mP->region_attr = db.get_attribute(region_attr_name);
}

void
FlatExclusiveRegionProfile::operator() (CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec)
{
    mP->process_record(db, rec);
}

std::tuple< std::map<std::string, double>, double, double >
FlatExclusiveRegionProfile::result() const
{
    return std::make_tuple(mP->reg_profile, mP->total_reg, mP->total);
}
