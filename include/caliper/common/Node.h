// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/** 
 * \file Node.h 
 * \brief Context tree node class.
 */

#ifndef CALI_NODE_H
#define CALI_NODE_H

#include "cali_types.h"

#include "IdType.h"
#include "Variant.h"

#include "util/lockfree-tree.hpp"

#include <atomic>

namespace cali 
{

/// \brief A context tree node.
///   Represents a context tree node and its (attribute key, value) pair.

class Node : public IdType, public util::LockfreeIntrusiveTree<Node> 
{
    util::LockfreeIntrusiveTree<Node>::Node m_treenode;

    cali_id_t m_attribute;
    Variant   m_data;

public:

    Node(cali_id_t id, cali_id_t attr, const Variant& data)
        : IdType(id),
          util::LockfreeIntrusiveTree<Node>(this, &Node::m_treenode), 
        m_attribute { attr },
        m_data      { data }
        { }

    Node(const Node&) = delete;

    Node& operator = (const Node&) = delete;

    ~Node();

    bool equals(cali_id_t attr, const Variant& v) const {
        return m_attribute == attr ? m_data == v : false;
    }

    cali_id_t attribute() const { return m_attribute; }
    Variant   data() const      { return m_data;      }    
};

} // namespace cali

#endif
