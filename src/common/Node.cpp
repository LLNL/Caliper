/// @file Node.cpp
/// Node class implementation

#include "Node.h"

#include "Attribute.h"

#include <cstring>

using namespace cali;
using namespace std;

namespace
{
    const char* NodeRecordElements[] = { "id", "attr", "data", "parent" };
}

const RecordDescriptor Node::s_record { 0x100, "node", 4, ::NodeRecordElements };

Node::~Node()
{
    unlink();
}

bool Node::equals(cali_id_t attr, const void* data, size_t size) const
{
    if (m_attribute == attr)
        return m_data == Variant(m_data.type(), data, size);

    return false;
}

void Node::push_record(WriteRecordFn rec) const
{
    cali_id_t parent_id = parent() ? parent()->id() : CALI_INV_ID;

    int n[] = { 1, 1, 1, (parent_id == CALI_INV_ID ? 0 : 1) };

    Variant  v_id     { id()        };
    Variant  v_attr   { m_attribute }; 
    Variant  v_parent { parent_id   };

    const Variant* data[] = { &v_id, &v_attr, &m_data, &v_parent };

    rec(s_record, n, data);
}

RecordMap Node::record() const
{
    RecordMap recmap = {
        { "id",        { id() }          },
        { "attribute", { m_attribute }   },
        { "data",      { m_data }        } };

    if (m_data.type() != CALI_TYPE_INV)
        recmap.insert(make_pair( "type",   Variant(m_data.type())  ));
    if (parent() && parent()->id() != CALI_INV_ID)
        recmap.insert(make_pair( "parent", Variant(parent()->id()) ));

    return recmap;
}
