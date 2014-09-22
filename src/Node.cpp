/// @file Node.cpp
/// Node class implementation

#include "Node.h"

#include <cstring>

using namespace cali;
using namespace std;


Node::Node(ctx_id_t id, ctx_id_t attr, void* data, size_t size)
    : IdType(id),
      util::IntrusiveTree<Node>(this, &Node::m_treenode),
      m_attribute(attr), m_datasize(size), m_data(data)
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
