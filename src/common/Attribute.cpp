// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Attribute class implementation

#include "caliper/common/Attribute.h"

#include "caliper/common/Node.h"

using namespace cali;
using namespace std;

Attribute
Attribute::make_attribute(Node* node)
{
    // sanity check: make sure we have the necessary attributes (name and type)

    // Given node must be attribute name

    if (!node || node->attribute() == CALI_INV_ID || node->attribute() != NAME_ATTR_ID)
        return Attribute::invalid;

    // Find type attribute
    for (const Node* p = node; p && p->attribute() != CALI_INV_ID; p = p->parent())
        if (p->attribute() == TYPE_ATTR_ID)
            return Attribute(node);

    return Attribute::invalid;
}

std::string
Attribute::name() const
{
    for (const Node* node = m_node; node; node = node->parent())
        if (node->attribute() == NAME_ATTR_ID)
            return node->data().to_string();

    return std::string();
}

const char*
Attribute::name_c_str() const
{
    for (const Node* node = m_node; node; node = node->parent())
        if (node->attribute() == NAME_ATTR_ID)
            return static_cast<const char*>(node->data().data());

    return nullptr;
}

cali_attr_type
Attribute::type() const
{
    for (const Node* node = m_node; node; node = node->parent())
        if (node->attribute() == TYPE_ATTR_ID)
            return node->data().to_attr_type();

    return CALI_TYPE_INV;
}

int
Attribute::properties() const
{
    for (const Node* node = m_node; node; node = node->parent())
        if (node->attribute() == PROP_ATTR_ID)
            return node->data().to_int();

    return CALI_ATTR_DEFAULT;
}

Variant
Attribute::get(const Attribute& attr) const
{
    for (const Node* node = m_node; node; node = node->parent())
        if (node->attribute() == attr.id())
            return node->data();

    return Variant();
}

std::ostream&
cali::operator << (std::ostream& os, const Attribute& a)
{
    char buf[256];

    cali_prop2string(a.properties(), buf, 256);

    return os << "{ \"id\" : " << a.id()
              << ", \"name\" : \"" << a.name() << "\""
              << ", \"type\" : \"" << cali_type2string(a.type()) << "\""
              << ", \"properties\" : \"" << buf << "\" }";
}

const Attribute Attribute::invalid { nullptr };
