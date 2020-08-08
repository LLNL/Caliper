// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/**
 * \file Node.h
 * \brief Metadata tree node class.
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

/// \brief A metadata tree node.
///   Represents a metadata tree node and its (attribute key, value) pair.

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

    /// \brief Check if the node's attribute and value are equal to
    ///   \a attr and \a v
    bool equals(cali_id_t attr, const Variant& v) const {
        return m_attribute == attr ? m_data == v : false;
    }

    /// \brief Return the node's attribute ID
    cali_id_t attribute() const { return m_attribute; }
    /// \brief Return the node's data element
    Variant   data() const      { return m_data;      }
};

} // namespace cali

#endif
