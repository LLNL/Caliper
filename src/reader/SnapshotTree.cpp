// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// SnapshotTree class implementation

#include "caliper/reader/SnapshotTree.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"
#include "caliper/common/Node.h"

#include <algorithm>
#include <cassert>
#include <vector>

using namespace cali;


struct SnapshotTree::SnapshotTreeImpl
{
    SnapshotTreeNode* m_root;

    void recursive_delete(SnapshotTreeNode* node) {
        if (node) {
            recursive_delete(node->first_child());
            recursive_delete(node->next_sibling());

            delete node;
        }
    }

    const SnapshotTreeNode*
    add_snapshot(const CaliperMetadataAccessInterface& db,
                 const EntryList&  list,
                 IsPathPredicateFn is_path)
    {
        std::vector< std::pair<Attribute, Variant> > path;
        std::vector< std::pair<Attribute, Variant> > data;
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
                auto it = std::find_if(data.begin(), data.end(), [&attr](const std::pair<Attribute, Variant>& p){
                        return attr == p.first;
                    });

                if (it == data.end())
                    data.push_back(std::make_pair(attr, val));
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
                child = new SnapshotTreeNode((*it).first, (*it).second);
                node->append(child);
            }

            node = child;
        }

        assert(node);

        //
        // add the attributes
        //

        node->add_record(data);

        return node;
    }

    SnapshotTreeImpl(const Attribute& attr, const Variant& value)
        : m_root(new SnapshotTreeNode(attr, value))
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
