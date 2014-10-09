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

class AttributeReader;


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

    void      foreach_attribute(std::function<void(const Attribute&)> proc) const;

    /// Resets store and reads attributes from @param r. 
    void      read(AttributeReader& r);
};

}; // namespace cali

#endif
