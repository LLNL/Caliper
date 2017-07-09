// Copyright (c) 2017, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file  SnapshotTree.h
/// \brief Classes to organize a set of snapshots in tree form
///
/// A snapshot tree organizes Caliper snapshot records in a tree based
/// on the context tree hierarchy information in one or more entries
/// in the record. This hierarchy information comes from nested
/// `begin`/`end` regions, or from list assignments. Typical examples
/// are nested annotations, or call paths determined by stack
/// unwinding.
///
/// To build a snapshot tree, users must select one or more _path_
/// attributes from each snapshot record. The `SnapshotTree` class
/// merges paths from multiple snapshot records to form a tree. The
/// non-path snapshot record entries are added as _attributes_ to the
/// record's snapshot tree node.

#pragma once

#include "RecordProcessor.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Variant.h"

#include "caliper/common/util/lockfree-tree.hpp"

#include <functional>
#include <map>
#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;


//
// --- SnapshotTree class
//

/// A Snapshot tree node
///
/// A snapshot tree node represents a node in the snapshot tree.
/// The node contains the key/value of the path attribute, and - if
/// the node represents a snapshot - a key/value map of the
/// associated snapshot record's non-path attributes.
///
/// A node that does _not_ represent a snapshot record (but lies on
/// the path) is considered _empty_.
///
/// Iterate over a node's children with `first_child()`/`next_sibling()`:
///
/// ```
/// std::cout << node->label_value().to_string() << "'s children: ";
/// const SnapshotTreeNode* child = node->first_child();
/// for ( ; child; child = child->next_sibling()
///     std::cout << child->label_value().to_string() << " ";
/// ```
///
/// Obtain the parent node with `parent()`
///
/// ```
/// std::cout << "Path to root: "
/// for (SnapshotTreeNode* node = ... ; node; node = node->parent())
///     std::cout << node->label_value().to_string() << " ";
/// ```

class SnapshotTreeNode : public util::LockfreeIntrusiveTree<SnapshotTreeNode>
{
    util::LockfreeIntrusiveTree<SnapshotTreeNode>::Node m_treenode;

    Attribute m_label_key;
    Variant   m_label_value;

    bool      m_empty;

    std::map<cali::Attribute, cali::Variant> m_attributes;

    void assign_attributes(std::map<cali::Attribute, cali::Variant>&& a) {
        m_attributes = a;
        m_empty = false;
    }

    SnapshotTreeNode(const Attribute& label_key, const Variant& label_val, bool empty = true)
        : util::LockfreeIntrusiveTree<SnapshotTreeNode>(this, &SnapshotTreeNode::m_treenode),
          m_label_key(label_key),
          m_label_value(label_val),
          m_empty(empty)
    { }

public:

    /// Return the label attribute key.
    Attribute label_key()   const { return m_label_key;   }
    /// Return the label value.
    Variant   label_value() const { return m_label_value; }

    /// Return `false` if the node represents a snapshot record,
    /// otherwise (i.e., if the node is empty) return `true`.
    bool      is_empty()    const { return m_empty;       }

    /// Return `true` if the label equals the given (\a key,\a value) pair.
    bool label_equals(const Attribute& key, const Variant& value) const {
        return m_label_key == key && m_label_value == value;
    }

    /// Access the non-path attributes of the snapshot associated
    /// with this node.
    const std::map<cali::Attribute, cali::Variant>& attributes() const {
        return m_attributes;
    }

    friend class SnapshotTree;
}; // SnapshotTreeNode


//
// --- SnapshotTree
//

/// \class SnapshotTree
/// \brief Build up and access a snapshot tree

class SnapshotTree
{
    struct SnapshotTreeImpl;
    std::shared_ptr<SnapshotTreeImpl> mP;

public:

    /// Constructor. Creates a default root node.
    SnapshotTree();

    /// Constructor. Create root node with label (\a attr, \a value).
    SnapshotTree(const Attribute& attr, const Variant& value);

    ~SnapshotTree();

    /// A predicate to determine if a given _(attribute,value)_ pair
    /// in a snapshot record belongs to the tree path or not.
    typedef std::function<bool(const Attribute&,const Variant&)> IsPathPredicateFn;

    /// Add given snapshot record to the tree.
    ///
    /// Insert a given snapshot record to the tree. This function
    /// unpacks each (attribute,value) pair in the snapshot record,
    /// and uses the \a is_path predicate to find the path entries
    /// among them. All remaining top-level entries in the snapshot
    /// record are added as _attributes_ to an existing `empty` node
    /// with the identified path (this node then becomes `occupied`),
    /// or a new node if no empty node with the given path exists.
    ///
    /// \param db      A Caliper metadata manager.
    /// \param list    The snapshot record.
    /// \param is_path Predicate to determine if an
    ///    (attribute,value) pair is on the path or not.
    /// \return Pointer to the newly created tree node. Null pointer
    ///    if no path entry was found.
    const SnapshotTreeNode*
    add_snapshot(const CaliperMetadataAccessInterface& db,
                 const EntryList&  list,
                 IsPathPredicateFn is_path);

    /// Return the snapshot tree's root node.
    const SnapshotTreeNode*
    root() const;
};

} // namespace cali
