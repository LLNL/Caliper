/// @file Node.cpp
/// Node class implementation

#include "Node.h"

#include "Attribute.h"

#include <cstring>

using namespace cali;
using namespace std;


Node::Node(ctx_id_t id, const Attribute& attr, void* data, size_t size)
    : IdType(id),
      util::IntrusiveTree<Node>(this, &Node::m_treenode),
      m_attribute(attr.id()), m_type(attr.type()), m_datasize(size), m_data(data)
{
}

Node::~Node()
{
    unlink();
}

bool Node::equals(ctx_id_t attr, const void* data, size_t size) const
{
    if (m_attribute == attr && m_datasize == size)
        return (0 == memcmp(m_data, data, size));

    return false;
}

RecordMap Node::record() const
{
    RecordMap recmap = {
        { "id",        { id() }                       },
        { "attribute", { m_attribute }                },
        { "type",      { m_type }                     },
        { "data",      { m_type, m_datasize, m_data } } };

    if (parent() && parent()->id() != CTX_INV_ID)
        recmap.insert(make_pair( "parent", Variant(parent()->id()) ));

    return recmap;
}
