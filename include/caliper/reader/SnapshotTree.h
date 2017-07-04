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
/// \brief Organize set of snapshots in tree form 

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

public:

    SnapshotTreeNode(const Attribute& label_key, const Variant& label_val, bool empty = true)
        : util::LockfreeIntrusiveTree<SnapshotTreeNode>(this, &SnapshotTreeNode::m_treenode), 
          m_label_key(label_key), 
          m_label_value(label_val), 
          m_empty(empty)
    { }

    Attribute label_key()   const { return m_label_key;   }
    Variant   label_value() const { return m_label_value; } 

    bool      is_empty()    const { return m_empty;       }

    bool label_equals(const Attribute& key, const Variant& value) const {
        return m_label_key == key && m_label_value == value;
    }

    const std::map<cali::Attribute, cali::Variant>& attributes() const {
        return m_attributes;
    }

    friend class SnapshotTree;
}; // SnapshotTreeNode


//
// --- SnapshotTree
//

class SnapshotTree
{
    struct SnapshotTreeImpl;
    std::shared_ptr<SnapshotTreeImpl> mP;

public:

    SnapshotTree();

    SnapshotTree(const Attribute& attr, const Variant& value);

    ~SnapshotTree();

    typedef std::function<bool(const Attribute&,const Variant&)> IsPathPredicateFn;

    const SnapshotTreeNode* 
    add_snapshot(const CaliperMetadataAccessInterface& db,
                 const EntryList&  list,
                 IsPathPredicateFn is_path);

    const SnapshotTreeNode*
    root() const;
};

} // namespace cali
