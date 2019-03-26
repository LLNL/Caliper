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

// Pretty-print tree-organized snapshots 

#include "caliper/reader/TreeFormatter.h"

#include "caliper/reader/QuerySpec.h"
#include "caliper/reader/SnapshotTree.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

#include "caliper/common/Attribute.h"
#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/StringConverter.h"

#include "caliper/common/util/split.hpp"

#include "../common/util/format_util.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <mutex>
#include <utility>

using namespace cali;


struct TreeFormatter::TreeFormatterImpl
{
    SnapshotTree             m_tree;

    QuerySpec::AttributeSelection m_attribute_columns;

    struct ColumnInfo {
        std::string display_name;
        int width;
    };
    
    std::map<Attribute, ColumnInfo> m_column_info;

    int                      m_path_column_width;
    int                      m_max_column_width;

    std::map<std::string, std::string> m_aliases;

    std::vector<std::string> m_path_key_names;
    std::vector<Attribute>   m_path_keys;

    std::mutex               m_path_key_lock;


    void configure(const QuerySpec& spec) {
        // set path keys (first argument in spec.format.args)
        if (spec.format.args.size() > 0)
            util::split(spec.format.args.front(), ',',
                        std::back_inserter(m_path_key_names));

        // set max column width
        if (spec.format.args.size() > 1) {
            bool ok = false;
            
            m_max_column_width = StringConverter(spec.format.args[1]).to_int(&ok);

            if (!ok) {
                Log(0).stream() << "TreeFormatter: invalid column width argument \"" 
                                << spec.format.args[1] << "\""
                                << std::endl;
                
                m_max_column_width = -1;
            }
        }
        
        m_path_keys.assign(m_path_key_names.size(), Attribute::invalid);
        m_attribute_columns = spec.attribute_selection;
        m_aliases = spec.aliases;
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
        const SnapshotTreeNode* node = nullptr;

        if (m_path_key_names.empty()) {
            node = m_tree.add_snapshot(db, list, [](const Attribute& attr,const Variant&){
                    return attr.is_nested();
                });
        } else { 
            auto path_keys = get_path_keys(db);

            node = m_tree.add_snapshot(db, list, [&path_keys](const Attribute& attr, const Variant&){
                    return (std::find(std::begin(path_keys), std::end(path_keys), 
                                      attr) != std::end(path_keys));
                });
        }
        
        if (!node)
            return;

        // update column widths

        {
            int len = node->label_value().to_string().size();

            for (const SnapshotTreeNode* n = node; n && n->label_key() != Attribute::invalid; n = n->parent())
                len += 2;

            m_path_column_width = std::max(m_path_column_width, len);
        }

        for (auto &p : node->attributes()) {
            int len = p.second.to_string().size();
            auto it = m_column_info.find(p.first);

            if (it == m_column_info.end()) {
                std::string name = p.first.name();
                                
                auto ait = m_aliases.find(name);
                if (ait != m_aliases.end())
                    name = ait->second;

                ColumnInfo ci { name, std::max<int>(len, name.size()) };

                m_column_info.insert(std::make_pair(p.first, ci));
            } else
                it->second.width = std::max(it->second.width, len);
        }
    }

    void recursive_print_nodes(const SnapshotTreeNode* node, 
                               int level, 
                               const std::vector<Attribute>& attributes, 
                               std::ostream& os)
    {
        // 
        // print this node
        //

        std::string path_str;
        path_str.append(std::min(2*level, m_path_column_width-2), ' ');
        
        if (2*level >= m_path_column_width)
            path_str.append("..");
        else
            path_str.append(util::clamp_string(node->label_value().to_string(), m_path_column_width-2*level));

        util::pad_right(os, path_str, m_path_column_width);

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
                util::pad_left (os, str, m_column_info[a].width);
            else
                util::pad_right(os, str, m_column_info[a].width);
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

        if (m_max_column_width > 0)
            m_path_column_width = std::min(m_path_column_width, std::max(4, m_max_column_width));
        
        //
        // establish order of attribute columns
        //

        std::vector<Attribute> attributes;

        switch (m_attribute_columns.selection) {
        case QuerySpec::AttributeSelection::Default:
            // auto-attributes: skip hidden and "cali." attributes
            for (auto &p : m_column_info) {
                if (p.first.is_hidden())
                    continue;
                if (p.first.name().compare(0, 5, "cali.") == 0)
                    continue;

                attributes.push_back(p.first);
            }
            break;
        case QuerySpec::AttributeSelection::All:
            for (auto &p : m_column_info)
                attributes.push_back(p.first);
            break;
        case QuerySpec::AttributeSelection::List:
            for (const std::string& s : m_attribute_columns.list) {
                Attribute attr = db.get_attribute(s);

                if (attr == Attribute::invalid)
                    std::cerr << "cali-query: TreeFormatter: Attribute \"" << s << "\" not found."
                              << std::endl;
                else
                    attributes.push_back(attr);
            }
            break;
        case QuerySpec::AttributeSelection::None:
            // keep empty list
            break;
        }

        //
        // print header
        //

        util::pad_right(os, "Path", m_path_column_width);

        for (const Attribute& a : attributes)
            util::pad_right(os, m_column_info[a].display_name, m_column_info[a].width);

        os << std::endl;

        //
        // print tree nodes
        //

        const SnapshotTreeNode* node = m_tree.root();

        if (node)
            for (node = node->first_child(); node; node = node->next_sibling())
                recursive_print_nodes(node, 0, attributes, os);
    }

    TreeFormatterImpl()
        : m_path_column_width(0),
          m_max_column_width(100)
    { }
};


TreeFormatter::TreeFormatter(const QuerySpec& spec)
    : mP { new TreeFormatterImpl }
{
    mP->configure(spec);
}

TreeFormatter::~TreeFormatter()
{
    mP.reset();
}

void
TreeFormatter::process_record(CaliperMetadataAccessInterface& db, const EntryList& list)
{
    mP->add(db, list);
}

void
TreeFormatter::flush(CaliperMetadataAccessInterface& db, std::ostream& os)
{
    mP->flush(db, os);
}
