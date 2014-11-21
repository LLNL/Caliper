/** 
 * @file attribute.h 
 * AttributeStore class declaration
 */

#ifndef CALI_ATTRIBUTESTORE_H
#define CALI_ATTRIBUTESTORE_H

#include "Attribute.h"

#include <memory>
#include <string>


namespace cali 
{

class AttributeStore 
{
    struct AttributeStoreImpl;
    
    std::unique_ptr<AttributeStoreImpl> mP;
    
public:

    AttributeStore();

    ~AttributeStore();

    size_t    size() const;

    Attribute get(cali_id_t id) const;
    Attribute get(const std::string& name) const;

    Attribute create(const std::string&  name, cali_attr_type type, int properties);

    void      foreach_attribute(std::function<void(const Attribute&)> proc) const;
};

}; // namespace cali

#endif
