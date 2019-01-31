// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/** 
 * \file Attribute.h 
 * \brief Attribute class declaration
 */

#ifndef CALI_ATTRIBUTE_H
#define CALI_ATTRIBUTE_H

#include "caliper/common/cali_types.h"

#include "caliper/common/Node.h"
#include "caliper/common/Variant.h"

#include <iostream>
#include <string>

namespace cali
{
    
struct MetaAttributeIDs {
    cali_id_t name_attr_id;
    cali_id_t type_attr_id;
    cali_id_t prop_attr_id;

    static const MetaAttributeIDs invalid;
};    

/// \brief Encapsulate an attribute key.
///
/// All attribute meta-information (e.g., type, property flags, name) is
/// stored in the context tree. An attribute key is a context tree 
/// reference to a \a cali.attribute.name node. This class encapsulates
/// an attribute key node and provides access to the attribute's
/// metadata.

class Attribute
{

public:

    constexpr Attribute()
        : m_node(0)
        { }

    cali_id_t      id() const { return m_node ? m_node->id() : CALI_INV_ID; }

    std::string    name() const;
    const char*    name_c_str() const;
    
    cali_attr_type type() const;

    int            properties() const;

    /// \brief Return the context tree node pointer that represents 
    ///   this attribute key.
    const Node*    node() const {
        return m_node;
    }

    bool store_as_value() const { 
        return properties() & CALI_ATTR_ASVALUE; 
    }
    bool is_autocombineable() const   { 
        return !store_as_value() && !(properties() & CALI_ATTR_NOMERGE);
    }
    bool skip_events() const {
        return properties() & CALI_ATTR_SKIP_EVENTS;
    }
    bool is_hidden() const {
        return properties() & CALI_ATTR_HIDDEN;
    }
    bool is_nested() const {
        return properties() & CALI_ATTR_NESTED;
    }
    bool is_global() const {
        return properties() & CALI_ATTR_GLOBAL;
    }

    Variant        get(const Attribute& attr) const;
    
    static Attribute make_attribute(const Node* node);

    static MetaAttributeIDs meta_attribute_keys() {
        return s_keys;
    }

    static const Attribute invalid;

private:

    const Node*            m_node;

    static const MetaAttributeIDs s_keys;

    Attribute(const Node* node)
        : m_node(node)
        { }

    friend bool operator <  (const cali::Attribute& a, const cali::Attribute& b);
    friend bool operator == (const cali::Attribute& a, const cali::Attribute& b);
    friend bool operator != (const cali::Attribute& a, const cali::Attribute& b);
};

inline bool operator < (const cali::Attribute& a, const cali::Attribute& b) {
    return a.id() < b.id();
}

inline bool operator == (const cali::Attribute& a, const cali::Attribute& b) {
    // we don't have copies of nodes, so the ptr should be unique
    return a.m_node == b.m_node;
}

inline bool operator != (const cali::Attribute& a, const cali::Attribute& b) {
    return a.m_node != b.m_node;
}

std::ostream& operator << (std::ostream&, const cali::Attribute&);

} // namespace cali

#endif // CALI_ATTRIBUTE_H
