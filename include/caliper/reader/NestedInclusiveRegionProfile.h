// Copyright (c) 2019, Lawrence Livermore National Security, LLC.  
// See top-level LICENSE file for details.

/// \file NestedInclusiveRegionProfile.h
/// NestedInclusiveRegionProfile class

#pragma once

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;
class Entry;

/// \brief Calculate a flat exclusive region profile
class NestedInclusiveRegionProfile {
    struct NestedInclusiveRegionProfileImpl;
    std::shared_ptr<NestedInclusiveRegionProfileImpl> mP;

public:
    
    NestedInclusiveRegionProfile(CaliperMetadataAccessInterface& db,
                               const char* metric_attr_name,
                               const char* region_attr_name = "");

    void operator ()(CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec);

    /// \brief Return tuple with { { region name -> value } map, sum in given region type, total sum }
    std::tuple< std::map<std::string, double>, double, double >
    result() const;
};

} // namespace cali
