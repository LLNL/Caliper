// Copyright (c) 2016, Lawrence Livermore National Security, LLC.
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

/// \file TreeFormatter.cpp
/// Pretty-print tree-organized snapshots 

#include "caliper/reader/TreeFormatter.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/ContextRecord.h"
#include "caliper/common/Node.h"
#include "caliper/common/StringConverter.h"

#include "caliper/common/util/split.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <mutex>
#include <utility>

using namespace cali;

namespace
{

//
// --- OutputTree class
//

class OutputTreeNode : public util::LockfreeIntrusiveTree<OutputTreeNode> 
{
    util::LockfreeIntrusiveTree<OutputTreeNode>::Node m_treenode;

    Attribute m_label_key;
    Variant   m_label_value;

    bool      m_empty;

    std::map<cali::Attribute, cali::Variant> m_attributes;

    void add_attribute(const cali::Attribute& attr, const cali::Variant& value) {
        m_attributes[attr] = value;
        m_empty = false;
    }

public:

    OutputTreeNode(const Attribute& label_key, const Variant& label_val, bool empty = true)
        : util::LockfreeIntrusiveTree<OutputTreeNode>(this, &OutputTreeNode::m_treenode), 
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

    friend class OutputTree;
};    

/// \brief Construct a tree structure from a snapshot record stream
class OutputTree
{
    OutputTreeNode* m_root;

    void recursive_delete(OutputTreeNode* node) {
        if (node) {
            node = node->first_child();

            while (node) {
                ::OutputTreeNode* tmp = node->next_sibling();

                recursive_delete(node);

                delete node;
                node = tmp;
            }
        }
    }
    
public:

    OutputTree()
        : m_root(new OutputTreeNode(Attribute::invalid, Variant(), true))
    { }

    OutputTree(const Attribute& attr, const Variant& value)
        : m_root(new OutputTreeNode(attr, value, true))
    { } 


    ~OutputTree() {
        recursive_delete(m_root);
    }

    const OutputTreeNode* add(const CaliperMetadataAccessInterface& db, 
                              const EntryList& list,
                              std::function<bool(const Attribute&,const Variant&)> is_path)
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

        OutputTreeNode* node = m_root;

        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            OutputTreeNode* child = node->first_child();

            for ( ; child && !child->label_equals((*it).first, (*it).second); child = child->next_sibling())
                ;

            if (!child) {
                child = new OutputTreeNode((*it).first, (*it).second, true /* empty */);
                node->append(child);
            }

            node = child;
        }

        assert(node);

        if (!(node->is_empty())) {
            assert(node->parent());

            // create a new node if the existing one is not empty
            node->parent()->append(new OutputTreeNode(node->label_key(), 
                                                      node->label_value(), 
                                                      false));
        }

        //
        // add the attributes
        // 

        node->m_attributes = std::move(attributes);

        return node;
    }

    const OutputTreeNode* root() const { return m_root; }
};

const char whitespace[120+1] =
    "                                        "
    "                                        "
    "                                        ";

inline std::ostream& pad_right(std::ostream& os, const std::string& str, std::size_t width)
{
    os << str << whitespace+(120-std::min<std::size_t>(120, 1+width-str.size()));
    return os;
}

inline std::ostream& pad_left (std::ostream& os, const std::string& str, std::size_t width)
{
    os << whitespace+(120 - std::min<std::size_t>(120, width-str.size())) << str << ' ';
    return os;
}

} // namespace [anonymous]


struct TreeFormatter::TreeFormatterImpl
{
    ::OutputTree             m_tree;

    std::vector<std::string> m_attribute_column_names;
    std::map<Attribute, int> m_attribute_column_widths;

    int                      m_path_column_width;

    std::vector<std::string> m_path_key_names;
    std::vector<Attribute>   m_path_keys;

    std::mutex               m_path_key_lock;


    void parse(const std::string& path_key_str, const std::string& attribute_str) {
        util::split(path_key_str,  ':', std::back_inserter(m_path_key_names));
        util::split(attribute_str, ':', std::back_inserter(m_attribute_column_names));

        m_path_keys.assign(m_path_key_names.size(), Attribute::invalid);
    }

    std::vector<Attribute> get_path_keys(const CaliperMetadataAccessInterface& db) {
        std::vector<Attribute> path_keys;

        {
            std::lock_guard<std::mutex>
                g(m_path_key_lock);

            path_keys = m_path_keys;
        }

        for (std::vector<Attribute>::size_type i = 0; i < path_keys.size(); ++i) 
            if (path_keys[i] == Attribute::invalid) {
                Attribute attr = db.get_attribute(m_path_key_names[i]);

                if (attr != Attribute::invalid) {
                    path_keys[i]   = attr;
                    std::lock_guard<std::mutex>
                        g(m_path_key_lock);
                    m_path_keys[i] = attr;
                }
            }

        return path_keys;
    }

    void add(const CaliperMetadataAccessInterface& db, const EntryList& list) {
        auto path_keys = get_path_keys(db);

        const OutputTreeNode* node = 
            m_tree.add(db, list, [&path_keys](const Attribute& attr, const Variant&){
                    return (std::find(std::begin(path_keys), std::end(path_keys), 
                                      attr) != std::end(path_keys));
                });

        if (!node)
            return;

        // update column widths

        {
            int len = node->label_value().to_string().size();

            for (const OutputTreeNode* n = node; n && n->label_key() != Attribute::invalid; n = n->parent())
                len += 2;

            m_path_column_width = std::max(m_path_column_width, len);
        }

        for (auto &p : node->attributes()) {
            int len = p.second.to_string().size();
            auto it = m_attribute_column_widths.find(p.first);

            if (it == m_attribute_column_widths.end())
                m_attribute_column_widths.insert(std::make_pair(p.first, std::max<int>(len, p.first.name().size())));
            else
                it->second = std::max(it->second, len);
        }
    }

    void recursive_print_nodes(const ::OutputTreeNode* node, 
                               int level, 
                               const std::vector<Attribute>& attributes, 
                               std::ostream& os)
    {
        // 
        // print this node
        //

        std::string path_str;
        path_str.assign(2*level, ' ');
        path_str.append(node->label_value().to_string());

        ::pad_right(os, path_str, m_path_column_width);

        for (const Attribute& a : attributes) {
            std::string str;

            {
                auto it = node->attributes().find(a);
                if (it != node->attributes().end())
                    str = it->second.to_string();
            }

            cali_attr_type t = a.type();
            bool align_right = (t == CALI_TYPE_INT    ||
                                t == CALI_TYPE_UINT   ||
                                t == CALI_TYPE_DOUBLE ||
                                t == CALI_TYPE_ADDR);

            if (align_right)
                ::pad_left (os, str, m_attribute_column_widths[a]);
            else
                ::pad_right(os, str, m_attribute_column_widths[a]);
        }

        os << std::endl;

        // 
        // recursively descend
        //

        for (node = node->first_child(); node; node = node->next_sibling())
            recursive_print_nodes(node, level+1, attributes, os);
    }

    void flush(const CaliperMetadataAccessInterface& db, std::ostream& os) {
        m_path_column_width = std::max<std::size_t>(m_path_column_width, 4 /* strlen("Path") */);

        //
        // establish order of attribute columns
        //

        std::vector<Attribute> attributes;

        if (m_attribute_column_names.size() > 0) {
            for (const std::string& s : m_attribute_column_names) {
                Attribute attr = db.get_attribute(s);

                if (attr == Attribute::invalid)
                    std::cerr << "cali-query: TreeFormatter: Attribute \"" << s << "\" not found."
                              << std::endl;
                else
                    attributes.push_back(attr);
            }
        } else {
            // auto-attributes: skip hidden and "cali." attributes
            for (auto &p : m_attribute_column_widths) {
                if (p.first.is_hidden())
                    continue;
                if (p.first.name().compare(0, 5, "cali.") == 0)
                    continue;

                attributes.push_back(p.first);
            }
        }

        //
        // print header
        //

        ::pad_right(os, "Path", m_path_column_width);

        for (const Attribute& a : attributes)
            ::pad_right(os, a.name(), m_attribute_column_widths[a]);

        os << std::endl;

        //
        // print tree nodes
        //

        const ::OutputTreeNode* node = m_tree.root();

        if (node)
            for (node = node->first_child(); node; node = node->next_sibling())
                recursive_print_nodes(node, 0, attributes, os);
    }

    TreeFormatterImpl()
        : m_path_column_width(0)
    { }
};


TreeFormatter::TreeFormatter(const std::string& path_keys, const std::string& attr_columns)
    : mP { new TreeFormatterImpl }
{
    mP->parse(path_keys, attr_columns);
}

TreeFormatter::~TreeFormatter()
{
    mP.reset();
}

void
TreeFormatter::operator()(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->add(db, list);
}

void
TreeFormatter::flush(CaliperMetadataAccessInterface& db, std::ostream& os)
{
    mP->flush(db, os);
}
