// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Entry class definition

#include "caliper/common/Entry.h"
#include "caliper/common/Node.h"

#include <functional>

using namespace cali;

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
                  
const Entry Entry::empty;
