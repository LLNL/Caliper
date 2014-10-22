/// @file Attribute.cpp
/// Attribute class implementation

#include "Attribute.h"

using namespace cali;
using namespace std;

Attribute::Attribute(ctx_id_t id, const std::string&  name, ctx_attr_type type, int prop)
    : IdType { id }, 
    m_name { name }, m_properties { prop }, m_type { type }
{ }

RecordMap Attribute::record() const
{
    const struct property_tbl_entry {
        ctx_attr_properties p; const char* str; const char* contstr;
    } property_tbl[] = { 
        { CTX_ATTR_ASVALUE, "value"   }, 
        { CTX_ATTR_NOMERGE, "nomerge" }, 
        { CTX_ATTR_GLOBAL,  "global"  }
    };

    int    propcount = 0;
    string propstr;

    for ( property_tbl_entry e : property_tbl )
        if (e.p & m_properties)
            propstr.append(propcount++ > 0 ? ":" : "").append(e.str);

    RecordMap recmap = { 
        { "id",   { id()   } },
        { "name", { m_name } },
        { "type", { m_type } } };

    if (propcount)
        recmap.insert(make_pair("properties", Variant(propstr)));

    return recmap;
}

const Attribute Attribute::invalid { CTX_INV_ID, "", CTX_TYPE_INV, CTX_ATTR_DEFAULT };
