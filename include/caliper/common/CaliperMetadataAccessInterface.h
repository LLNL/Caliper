// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file CaliperMetadataAccessInterface.h 
/// \brief Abstract Caliper metadata access interface

#pragma once

#include "Attribute.h"
#include "Entry.h"

#include <cstddef>
#include <vector>

namespace cali
{

class Node;

/// \brief Abstract base class for a Caliper metadata database.

class CaliperMetadataAccessInterface
{
public:

    // --- query operations

    virtual Node*
    node(cali_id_t) const = 0;

    virtual Attribute 
    get_attribute(cali_id_t id) const = 0;
    virtual Attribute
    get_attribute(const std::string& str) const = 0;

    /// \brief Return all attributes
    virtual std::vector<Attribute>
    get_all_attributes() const = 0;

    /// \brief Return all attributes that have a metadata entry \a meta, of any value
    std::vector<Attribute>
    find_attributes_with(const Attribute& meta) const;

    // --- modifying operations

    virtual Attribute
    create_attribute(const std::string& name,
                     cali_attr_type     type,
                     int                prop = CALI_ATTR_DEFAULT,
                     int                meta = 0,
                     const Attribute*   meta_attr = nullptr,
                     const Variant*     meta_data = nullptr) = 0;

    virtual Node* 
    make_tree_entry(std::size_t n, const Node* nodelist[], Node* parent = 0) = 0;

    // --- globals

    /// \brief Return global entries (entries with the \a CALI_ATTR_GLOBAL flag set)
    virtual std::vector<Entry>
    get_globals() = 0;
};

}
