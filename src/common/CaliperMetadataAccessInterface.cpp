// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file CaliperMetadataQueryInterface.cpp
/// Caliper metadata access interface implementation

#include "caliper/common/CaliperMetadataAccessInterface.h"

using namespace cali;

std::vector<Attribute>
CaliperMetadataAccessInterface::find_attributes_with(const Attribute& meta) const
{
    std::vector<Attribute> vec = get_all_attributes();
    std::vector<Attribute> ret;

    for (Attribute attr : vec)
        if (!attr.get(meta).empty())
            ret.push_back(attr);

    return ret;
}

std::vector<Attribute>
CaliperMetadataAccessInterface::find_attributes_with_prop(int prop) const
{
    std::vector<Attribute> vec = get_all_attributes();
    std::vector<Attribute> ret;

    for (Attribute attr : vec)
        if (attr.properties() & prop)
            ret.push_back(attr);

    return ret;
}