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

// SnapshotTree class implementation

#include "caliper/reader/SnapshotTree.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"

#include <cassert>
#include <vector>

using namespace cali;


struct SnapshotTree::SnapshotTreeImpl
{
    SnapshotTreeNode* m_root;

    void recursive_delete(SnapshotTreeNode* node) {
        if (node) {
            node = node->first_child();

            while (node) {
                ::SnapshotTreeNode* tmp = node->next_sibling();

                recursive_delete(node);

                delete node;
                node = tmp;
            }
        }
    }

    const SnapshotTreeNode* 
    add_snapshot(const CaliperMetadataAccessInterface& db, 
                 const EntryList&  list,
                 IsPathPredicateFn is_path) 
    {
        std::vector< std::pair<Attribute, Variant> > path;
        std::map<Attribute, Variant> attributes;

        //
        // helper function to distinguish path and attribute entries
        //

        auto push_entry = [&](cali_id_t attr_id, const Variant& val){
            Attribute attr = db.get_attribute(attr_id);

            if (attr == Attribute::invalid)
                return;

            if (is_path(attr, val))
                path.push_back(std::make_pair(attr, val));
            else {
                if (attributes.count(attr) == 0)
                    attributes.insert(std::make_pair(attr, val));
            }   
        };

        //
        // unpack snapshot; distinguish path and attribute entries
        //

        for (const Entry& e : list)
            if (e.is_immediate())
                push_entry(e.attribute(), e.value());
            else
                for (const Node* node = e.node(); node; node = node->parent())
                    if (node->id() != CALI_INV_ID)
                        push_entry(node->attribute(), node->data());

        if (path.empty())
            return nullptr;

        //
        // find / make path to the node
        //

        SnapshotTreeNode* node = m_root;

        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            SnapshotTreeNode* child = node->first_child();

            for ( ; child && !child->label_equals((*it).first, (*it).second); child = child->next_sibling())
                ;

            if (!child) {
                child = new SnapshotTreeNode((*it).first, (*it).second, true /* empty */);
                node->append(child);
            }

            node = child;
        }

        assert(node);

        if (!(node->is_empty())) {
            // create a new node if the existing one is not empty
            SnapshotTreeNode* parent = node->parent();

            node = new SnapshotTreeNode(node->label_key(), node->label_value(), false);
            parent->append(node);
        }

        //
        // add the attributes
        // 

        node->assign_attributes(std::move(attributes));

        return node;
    }

    SnapshotTreeImpl(const Attribute& attr, const Variant& value)
        : m_root(new SnapshotTreeNode(attr, value, true))
        { }

    ~SnapshotTreeImpl() {
        recursive_delete(m_root);
    }

}; // SnapshotTreeImpl


SnapshotTree::SnapshotTree()
    : mP(new SnapshotTreeImpl(Attribute::invalid, Variant()))
{ }

SnapshotTree::SnapshotTree(const Attribute& attr, const Variant& value)
    : mP(new SnapshotTreeImpl(attr, value))
{ }

SnapshotTree::~SnapshotTree()
{
    mP.reset();
}

const SnapshotTreeNode*
SnapshotTree::add_snapshot(const CaliperMetadataAccessInterface& db,
                           const EntryList&  list,
                           IsPathPredicateFn is_path)
{
    return mP->add_snapshot(db, list, is_path);
}

const SnapshotTreeNode*
SnapshotTree::root() const
{
    return mP->m_root;
}
