// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

#include "caliper/reader/NestedInclusiveRegionProfile.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include <algorithm>
#include <cstring>

using namespace cali;

namespace
{

std::string recursive_update(CaliperMetadataAccessInterface& db,
                             std::map<std::string, double>& reg_profile,
                             const Node* node,
                             double val,
                             cali_id_t r_a_id)
{
    std::string path;
    
    if (!node || node->attribute() == CALI_INV_ID)
        return path;
    
    path = recursive_update(db, reg_profile, node->parent(), val, r_a_id);

    cali_id_t n_a_id = node->attribute();
    bool is_target_reg =
        (r_a_id == CALI_INV_ID ? db.get_attribute(n_a_id).is_nested() : r_a_id == n_a_id);

    if (is_target_reg) {
        path.append(path.empty() ? "" : "/").append(node->data().to_string());
        reg_profile[path] += val;
    }

    return path;
}

} // namespace [anonymous]


struct NestedInclusiveRegionProfile::NestedInclusiveRegionProfileImpl
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
        std::string pathstr;
        
        for (const Entry& e : rec) {
            pathstr = ::recursive_update(db, reg_profile, e.node(), val, r_a_id);

            if (!pathstr.empty())
                break;
        }

        if (!pathstr.empty())
            total_reg += val;
    }
};


NestedInclusiveRegionProfile::NestedInclusiveRegionProfile(CaliperMetadataAccessInterface& db,
                                                       const char* metric_attr_name,
                                                       const char* region_attr_name)
    : mP { new NestedInclusiveRegionProfileImpl }
{
    mP->metric_attr = db.get_attribute(metric_attr_name);

    if (region_attr_name && strlen(region_attr_name) > 0)
        mP->region_attr = db.get_attribute(region_attr_name);
}

void
NestedInclusiveRegionProfile::operator() (CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec)
{
    mP->process_record(db, rec);
}

std::tuple< std::map<std::string, double>, double, double >
NestedInclusiveRegionProfile::result() const
{
    return std::make_tuple(mP->reg_profile, mP->total_reg, mP->total);
}
