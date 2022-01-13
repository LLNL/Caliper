// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Entry class definition

#include "caliper/common/Entry.h"
#include "caliper/common/Node.h"

using namespace cali;

int 
Entry::count(cali_id_t attr_id) const 
{
    int res = 0;

    if (is_immediate()) {
        res += (m_node->id() == attr_id ? 1 : 0);
    } else {
        for (const Node* node = m_node; node; node = node->parent())
            if (node->attribute() == attr_id)
                ++res;
    }

    return res;
}

Variant 
Entry::value(cali_id_t attr_id) const
{
    if (m_node && m_node->id() == attr_id)
        return m_value;

    for (const Node* node = m_node; node; node = node->parent())
        if (node->attribute() == attr_id)
	       return node->data();

    return Variant();
}
                  
const Entry Entry::empty;
