// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "AttributeExtract.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"

using namespace cali;

struct AttributeExtract::AttributeExtractImpl
{
    SnapshotProcessFn      m_snap_fn;
    Attribute              m_id_attr;

    static const cali_id_t s_attr_id; // The "attribute" attribute id

    void process_node(CaliperMetadataAccessInterface& db, const Node* node) {
        if (node->attribute() != s_attr_id)
            return;

        if (m_id_attr == Attribute::invalid)
            m_id_attr = db.create_attribute("attribute.id", CALI_TYPE_UINT, CALI_ATTR_ASVALUE);

        EntryList list { Entry(node), Entry(m_id_attr, node->id()) };

        m_snap_fn(db, list);
    }

    AttributeExtractImpl(SnapshotProcessFn snap_fn)
        : m_snap_fn(snap_fn),
          m_id_attr(Attribute::invalid)
        { }
};

const cali_id_t AttributeExtract::AttributeExtractImpl::s_attr_id = 8;

AttributeExtract::AttributeExtract(SnapshotProcessFn snap_fn)
    : mP { new AttributeExtractImpl(snap_fn) } 
{ }

AttributeExtract::~AttributeExtract()
{
    mP.reset();
}

void AttributeExtract::operator()(CaliperMetadataAccessInterface& db, const Node* node)
{
    mP->process_node(db, node);
}
