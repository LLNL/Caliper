/** 
 * @file Attribute.h 
 * Attribute class declaration
 */

#ifndef CALI_ATTRIBUTE_H
#define CALI_ATTRIBUTE_H

#include "cali_types.h"

#include "IdType.h"

#include <string>

namespace cali
{
    class AttributeStore;

    class Attribute : public IdType {
        std::string         m_name;
        ctx_attr_properties m_properties;
        ctx_attr_type       m_type;

        Attribute(ctx_id_t            id,
                  const std::string&  name, 
                  ctx_attr_properties properties,
                  ctx_attr_type       type);

        friend class AttributeStore;

    public:

        Attribute(const Attribute&) = default;

        Attribute(Attribute&&) = default;

        ctx_attr_type type() const {
            return m_type;
        }

        bool store_as_value() const { 
            return m_properties & CTX_ATTR_BYVALUE; 
        } 

        bool is_autocombineable() const   { 
            return !store_as_value() && (m_properties & CTX_ATTR_AUTOCOMBINE);
        } 

        bool clone() const {
            return !(m_properties & CTX_ATTR_NOCLONE);
        }

        static Attribute invalid;
    };

} // namespace cali

#endif // CALI_ATTRIBUTE_H
