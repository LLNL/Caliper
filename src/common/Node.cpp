/// @file Node.cpp
/// Node class implementation

#include "Node.h"

#include "Attribute.h"

#include <cstring>

using namespace cali;
using namespace std;


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
