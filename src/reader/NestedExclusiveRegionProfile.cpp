// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "caliper/reader/NestedExclusiveRegionProfile.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include <algorithm>
#include <cstring>

using namespace cali;

namespace
{

std::string get_path(CaliperMetadataAccessInterface& db, const Node* node, cali_id_t r_a_id)
{
    std::string path;
    
    if (!node || node->attribute() == CALI_INV_ID)
        return path;
    
    path = get_path(db, node->parent(), r_a_id);

    cali_id_t n_a_id = node->attribute();
    bool is_target_reg =
        (r_a_id == CALI_INV_ID ? db.get_attribute(n_a_id).is_nested() : r_a_id == n_a_id);

    if (is_target_reg)
        path.append(path.empty() ? "" : "/").append(node->data().to_string());

    return path;
}

} // namespace [anonymous]


struct NestedExclusiveRegionProfile::NestedExclusiveRegionProfileImpl
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

        cali_id_t r_a_id = region_attr.id();
        
        for (const Entry& e : rec) {
            if (!e.node())
                continue;
            
            cali_id_t n_a_id = e.attribute();
            bool is_target_reg =
                (r_a_id == CALI_INV_ID ? db.get_attribute(n_a_id).is_nested() : r_a_id == n_a_id);

            if (is_target_reg) {
                total_reg += val;
                reg_profile[::get_path(db, e.node(), r_a_id)] += val;
                break;
            }
        }
    }
};


NestedExclusiveRegionProfile::NestedExclusiveRegionProfile(CaliperMetadataAccessInterface& db,
                                                       const char* metric_attr_name,
                                                       const char* region_attr_name)
    : mP { new NestedExclusiveRegionProfileImpl }
{
    mP->metric_attr = db.get_attribute(metric_attr_name);

    if (region_attr_name && strlen(region_attr_name) > 0)
        mP->region_attr = db.get_attribute(region_attr_name);
}

void
NestedExclusiveRegionProfile::operator() (CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec)
{
    mP->process_record(db, rec);
}

std::tuple< std::map<std::string, double>, double, double >
NestedExclusiveRegionProfile::result() const
{
    return std::make_tuple(mP->reg_profile, mP->total_reg, mP->total);
}
