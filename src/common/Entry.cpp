/// @file Entry.cpp
/// Entry class definition

#include "Entry.h"
#include "Node.h"

#include <functional>

using namespace cali;

size_t
Entry::hash() const
{
    if (m_node)
        return m_node->id();

    size_t h1 = m_attr_id;
    size_t h2 = std::hash<std::string>()(m_value.to_string());

    return h1 ^ (h2 << 1);
}

cali_id_t
Entry::attribute() const
{
    return m_node ? m_node->attribute() : m_attr_id;
}

int 
Entry::count(cali_id_t attr_id) const 
{
    int res = 0;

    if (m_node) {
        for (const Node* node = m_node; node; node = node->parent())
            if (node->attribute() == attr_id)
                ++res;
    } else {
        if (m_attr_id != CALI_INV_ID && m_attr_id == attr_id)
            ++res;
    }

    return res;
}

Variant 
Entry::value() const
{
    return m_node ? m_node->data() : m_value;
}

Variant 
Entry::value(cali_id_t attr_id) const
{
    if (!m_node && attr_id == m_attr_id)
	return m_value;

    for (const Node* node = m_node; node; node = node->parent())
	if (node->attribute() == attr_id)
	    return node->data();

    return Variant();
}

bool cali::operator == (const Entry& lhs, const Entry& rhs)
{
    if (lhs.m_node)
        return rhs.m_node ? (lhs.m_node->id() == rhs.m_node->id()) : false;
    else if (rhs.m_node)
        return false;
    else if (lhs.m_attr_id == rhs.m_attr_id)
        return lhs.m_value == rhs.m_value;

    return false;
}

bool cali::operator <  (const Entry& lhs, const Entry& rhs)
{
    if (lhs.m_node)
        return rhs.m_node ? (lhs.m_node->id() < rhs.m_node->id()) : true;
    else if (rhs.m_node)
        return false;

    if (lhs.m_attr_id == rhs.m_attr_id)
        // slow but universal
        return lhs.m_value.to_string() < rhs.m_value.to_string();
    
    return lhs.m_attr_id < rhs.m_attr_id;
}
                  
const Entry Entry::empty;
