// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

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
