/** 
 * @file attribute.h 
 * AttributeStore class declaration
 */

#ifndef CALI_ATTRIBUTESTORE_H
#define CALI_ATTRIBUTESTORE_H

#include "Attribute.h"

#include <memory>
#include <string>
#include <utility>

namespace cali 
{

class AttributeStore 
{
    class AttributeStoreImpl;
    
    std::unique_ptr<AttributeStoreImpl> mP;
    
public:

    AttributeStore();

    ~AttributeStore();

    std::pair<bool, Attribute> get(ctx_id_t id) const;
    std::pair<bool, Attribute> get(const std::string& name) const;

    Attribute create(const std::string&  name, 
                     ctx_attr_properties properties = CTX_ATTR_AUTOCOMBINE,
                     ctx_attr_type       type = CTX_TYPE_USR);

};

}; // namespace cali

#endif
