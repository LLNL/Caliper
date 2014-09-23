/// @file Attribute.cpp
/// Attribute class implementation

#include "Attribute.h"

cali::Attribute::Attribute(ctx_id_t id, const std::string&  name, ctx_attr_type type, int prop)
    : IdType { id }, 
    m_name { name }, m_properties { prop }, m_type { type }
{ }

cali::Attribute cali::Attribute::invalid { CTX_INV_ID, "", CTX_TYPE_INV, CTX_ATTR_DEFAULT };
