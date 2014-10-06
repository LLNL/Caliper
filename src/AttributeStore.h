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

class AttributeWriter;

class AttributeStore 
{
    struct AttributeStoreImpl;
    
    std::unique_ptr<AttributeStoreImpl> mP;
    
public:

    AttributeStore();

    ~AttributeStore();

    Attribute get(ctx_id_t id) const;
    Attribute get(const std::string& name) const;

    Attribute create(const std::string&  name, ctx_attr_type type, int properties);

    void      write(AttributeWriter& w) const;
};

}; // namespace cali

#endif
