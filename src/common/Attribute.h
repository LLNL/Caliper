/** 
 * @file Attribute.h 
 * Attribute class declaration
 */

#ifndef CALI_ATTRIBUTE_H
#define CALI_ATTRIBUTE_H

#include "cali_types.h"

#include "IdType.h"
// #include "RecordMap.h"

#include <string>

namespace cali
{

class Attribute : public IdType 
{
    std::string    m_name;
    int            m_properties;
    cali_attr_type m_type;

public:

    Attribute(cali_id_t id, const std::string& name, cali_attr_type type, int prop = CALI_ATTR_DEFAULT)
        : IdType(id),
          m_name(name), m_properties(prop), m_type(type)
        { }

    std::string    name() const { return m_name; }
    cali_attr_type type() const { return m_type; }        

    int            properties() const { return m_properties; } 

    bool store_as_value() const { 
        return m_properties & CALI_ATTR_ASVALUE; 
    }
    bool is_autocombineable() const   { 
        return !store_as_value() && !(m_properties & CALI_ATTR_NOMERGE);
    }
    bool skip_events() const {
        return m_properties & CALI_ATTR_SKIP_EVENTS;
    }
    bool is_hidden() const {
        return m_properties & CALI_ATTR_HIDDEN;
    }

    // RecordMap record() const;

    static const Attribute invalid;
};

} // namespace cali

#endif // CALI_ATTRIBUTE_H
