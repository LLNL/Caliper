/** 
 * @file node.h 
 * Node class declaration
 */

#ifndef CALI_NODE_H
#define CALI_NODE_H

#include "cali_types.h"

#include "IdType.h"
#include "RecordMap.h"

#include "util/tree.hpp"


namespace cali 
{

class Attribute;

//
// --- Node base class 
//

class Node : public IdType, public util::IntrusiveTree<Node> 
{
    util::IntrusiveTree<Node>::Node m_treenode;

    ctx_id_t      m_attribute;
    ctx_attr_type m_type;

    std::size_t   m_datasize;
    void*         m_data;

public:

    Node(ctx_id_t id, const Attribute& attr, void* data, size_t size);

    Node(const Node&) = delete;

    Node& operator = (const Node&) = delete;

    ~Node();

    bool equals(ctx_id_t attr, const void* data, size_t size) const;

    ctx_id_t attribute() const { return m_attribute; }

    size_t size() const        { return m_datasize;  }
    const void* data() const   { return m_data;      }

    RecordMap record() const;
};

} // namespace cali

#endif
