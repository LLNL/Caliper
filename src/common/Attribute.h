/** 
 * @file Attribute.h 
 * Attribute class declaration
 */

#ifndef CALI_ATTRIBUTE_H
#define CALI_ATTRIBUTE_H

#include "cali_types.h"

#include <string>

namespace cali
{

class Node;

class Attribute
{

public:

    struct AttributeKeys {
        cali_id_t name_attr_id;
        cali_id_t type_attr_id;
        cali_id_t prop_attr_id;

        static const AttributeKeys invalid;
    };

    cali_id_t      id() const;

    std::string    name() const;
    cali_attr_type type() const;

    int            properties() const;

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

    static Attribute make_attribute(const Node* node, const AttributeKeys& keys);

    // RecordMap record() const;

    static const Attribute invalid;

private:

    const Node*   m_node;
    AttributeKeys m_keys;

    Attribute(const Node* node, const AttributeKeys& keys)
        : m_node(node), m_keys(keys)
        { }
};

inline bool operator < (const cali::Attribute& a, const cali::Attribute& b) {
    return a.id() < b.id();
}

inline bool operator == (const cali::Attribute& a, const cali::Attribute& b) {
    return a.id() == b.id();
}

inline bool operator != (const cali::Attribute& a, const cali::Attribute& b) {
    return a.id() != b.id();
}

} // namespace cali

#endif // CALI_ATTRIBUTE_H
