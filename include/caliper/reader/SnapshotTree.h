// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file  SnapshotTree.h
/// \brief Classes to organize a set of snapshots in tree form

#pragma once

#include "RecordProcessor.h"

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

/// \brief A Snapshot tree node
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
/// \sa SnapshotTree
/// \ingroup ReaderAPI

class SnapshotTreeNode : public util::LockfreeIntrusiveTree<SnapshotTreeNode>
{
    util::LockfreeIntrusiveTree<SnapshotTreeNode>::Node m_treenode;

    Attribute m_label_key;
    Variant   m_label_value;

    using Record = std::vector< std::pair<Attribute, Variant> >;

    std::vector<Record> m_records;

    void add_record(const Record& rec) {
        m_records.push_back(rec);
    }

    SnapshotTreeNode(const Attribute& label_key, const Variant& label_val)
        : util::LockfreeIntrusiveTree<SnapshotTreeNode>(this, &SnapshotTreeNode::m_treenode),
          m_label_key(label_key),
          m_label_value(label_val)
    { }

public:

    /// \brief Return the label attribute key.
    Attribute label_key()   const { return m_label_key;   }
    /// \brief Return the label value.
    Variant   label_value() const { return m_label_value; }

    /// \brief Return `false` if the node represents a snapshot record,
    ///   otherwise (i.e., if the node is empty) return `true`.
    bool      is_empty()    const { return m_records.empty(); }

    /// \brief Return `true` if the label equals the given (\a key,\a value) pair.
    bool label_equals(const Attribute& key, const Variant& value) const {
        return m_label_key == key && m_label_value == value;
    }

    /// \brief Access the non-path attributes of the snapshot records associated
    ///   with this node.
    const decltype(m_records)& records() const {
        return m_records;
    }

    friend class SnapshotTree;
}; // SnapshotTreeNode


//
// --- SnapshotTree
//

/// \brief Build up and access a snapshot tree
///
/// A snapshot tree organizes Caliper snapshot records in a tree based
/// on the context tree hierarchy information in one or more entries
/// in the record. This hierarchy information comes from nested
/// `begin`/`end` regions, or from list assignments. Typical examples
/// are nested annotations, or call paths determined by stack
/// unwinding.
///
/// To build a snapshot tree, users must select one or more _path_
/// attributes from each snapshot record. The SnapshotTree class
/// merges paths from multiple snapshot records to form a tree. The
/// non-path snapshot record entries are added as _attributes_ to the
/// record's snapshot tree node.
///
/// \sa SnapshotTreeNode
/// \ingroup ReaderAPI

class SnapshotTree
{
    struct SnapshotTreeImpl;
    std::shared_ptr<SnapshotTreeImpl> mP;

public:

    /// \brief Creates a default root node.
    SnapshotTree();

    /// \brief Create root node with label (\a attr, \a value).
    SnapshotTree(const Attribute& attr, const Variant& value);

    ~SnapshotTree();

    /// A predicate to determine if a given _(attribute,value)_ pair
    /// in a snapshot record belongs to the tree path or not.
    typedef std::function<bool(const Attribute&,const Variant&)> IsPathPredicateFn;

    /// \brief Add given snapshot record to the tree.
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

    /// \brief Return the snapshot tree's root node.
    const SnapshotTreeNode*
    root() const;
};

} // namespace cali
