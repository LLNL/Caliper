// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// Entry class definition

#include "caliper/common/Entry.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
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

Entry
Entry::get(const Attribute& attr) const
{
    if (empty())
        return Entry();

    cali_id_t attr_id = attr.id();

    if (m_node->id() == attr_id) // covers immediate entries
        return *this;
    else if (m_node->attribute() != Attribute::NAME_ATTR_ID) // this is a reference entry
        for (Node* node = m_node; node; node = node->parent())
            if (node->attribute() == attr_id)
                return Entry(node);

    return Entry();
}

Entry
Entry::unpack(const CaliperMetadataAccessInterface& db, const unsigned char* buffer, size_t* inc)
{
    size_t p = 0;
    Entry ret { db.node(vldec_u64(buffer, &p)) };

    if (ret.m_node->attribute() == Attribute::NAME_ATTR_ID)
        ret.m_value = Variant::unpack(buffer+p, &p);

    if (inc)
        *inc += p;

    return ret;
}
