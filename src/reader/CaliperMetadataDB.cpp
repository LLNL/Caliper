// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
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

/// @file CaliperMetadataDB.cpp
/// CaliperMetadataDB class definition

#include "CaliperMetadataDB.h"

#include <csv/CsvReader.h>

#include <Log.h>
#include <Node.h>
#include <RecordMap.h>
#include <StringConverter.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>

using namespace cali;
using namespace std;

namespace
{
    inline cali_id_t
    id_from_rec(const RecordMap& rec, const char* key) {
        auto it = rec.find(std::string(key));

        if (it != rec.end() && !it->second.empty()) {
            bool ok = false;
            cali_id_t id = StringConverter(it->second.front()).to_uint(&ok);

            if (ok)
                return id;
        }

        return CALI_INV_ID;
    }

    inline cali_id_t
    map_id_from_rec(const RecordMap& rec, const char* key, const IdMap& idmap) {
        cali_id_t id = id_from_rec(rec, key);

        if (id != CALI_INV_ID) {
            auto it = idmap.find(id);
            return it == idmap.end() ? id : it->second;
        }

        return CALI_INV_ID;
    }

    inline cali_id_t
    map_id_from_string(const std::string& str, const IdMap& idmap) {
        bool ok = false;
        cali_id_t id = StringConverter(str).to_uint(&ok);

        if (ok) {
            auto it = idmap.find(id);
            return it == idmap.end() ? id : it->second;
        }

        return CALI_INV_ID;
    }
} // namespace 

struct CaliperMetadataDB::CaliperMetadataDBImpl
{
    Node                      m_root;         ///< (Artificial) root node
    vector<Node*>             m_nodes;        ///< Node list
    mutable mutex             m_node_lock;

    Node*                     m_type_nodes[CALI_MAXTYPE+1] = { 0 };
    
    map<string, Node*>        m_attributes;
    mutable mutex             m_attribute_lock;

    vector<const char*>       m_string_db;
    
    void setup_bootstrap_nodes() {
        // Create initial nodes
        
        static const struct NodeInfo {
            cali_id_t id;
            cali_id_t attr_id;
            Variant   data;
            cali_id_t parent;
        }  bootstrap_nodes[] = {
            {  0, 9,  { CALI_TYPE_USR    }, CALI_INV_ID },
            {  1, 9,  { CALI_TYPE_INT    }, CALI_INV_ID },
            {  2, 9,  { CALI_TYPE_UINT   }, CALI_INV_ID },
            {  3, 9,  { CALI_TYPE_STRING }, CALI_INV_ID },
            {  4, 9,  { CALI_TYPE_ADDR   }, CALI_INV_ID },
            {  5, 9,  { CALI_TYPE_DOUBLE }, CALI_INV_ID },
            {  6, 9,  { CALI_TYPE_BOOL   }, CALI_INV_ID },
            {  7, 9,  { CALI_TYPE_TYPE   }, CALI_INV_ID },
            {  8, 8,  { CALI_TYPE_STRING, "cali.attribute.name",  19 }, 3 },
            {  9, 8,  { CALI_TYPE_STRING, "cali.attribute.type",  19 }, 7 },
            { 10, 8,  { CALI_TYPE_STRING, "cali.attribute.prop",  19 }, 1 },
            { CALI_INV_ID, CALI_INV_ID, { }, CALI_INV_ID },
        };

        // Create nodes

        m_nodes.resize(11);
        
        for (const NodeInfo* info = bootstrap_nodes; info->id != CALI_INV_ID; ++info) {
            Node* node = new Node(info->id, info->attr_id, info->data);

            m_nodes[info->id] = node;
                    
            if (info->parent != CALI_INV_ID)
                m_nodes[info->parent]->append(node);
            else
                m_root.append(node);
            
            if (info->attr_id == 9 /* type node */)
                m_type_nodes[info->data.to_attr_type()] = node;
            else if (info->attr_id == 8 /* attribute node*/)
                m_attributes.insert(make_pair(info->data.to_string(), node));
        }
    }

    Node* create_node(cali_id_t attr_id, const Variant& data, Node* parent) {
        // NOTE: We assume that m_node_lock is locked!
        
        Node* node = new Node(m_nodes.size(), attr_id, data);
        
        m_nodes.push_back(node);

        if (parent)
            parent->append(node);

        return node;
    }

    Variant make_variant(cali_attr_type type, const std::string& str) {
        Variant ret;

        switch (type) {
        case CALI_TYPE_INV:
            break;
        case CALI_TYPE_USR:
            ret = Variant(CALI_TYPE_USR, nullptr, 0);
            Log(0).stream() << "CaliperMetadataDB: Can't read USR data at this point" << std::endl;
            break;
        case CALI_TYPE_STRING:
            {
                auto it = std::upper_bound(m_string_db.begin(), m_string_db.end(),
                                           str.c_str(),
                                           [](const char* a, const char* b) { return strcmp(a, b) < 0; });

                if (it != m_string_db.end() && str == *it)
                    return Variant(CALI_TYPE_STRING, *it, str.size());

                char* ptr = new char[str.size() + 1];
                strcpy(ptr, str.c_str());

                m_string_db.insert(it, ptr);

                return Variant(CALI_TYPE_STRING, ptr, str.size());
            }
            break;
        case CALI_TYPE_INT:
            {
                bool ok = false;
                int  i  = StringConverter(str).to_int(&ok);

                if (ok)
                    ret = Variant(i);
            }
            break;
        case CALI_TYPE_ADDR:
        case CALI_TYPE_UINT:
            {
                bool ok = false;
                unsigned long u = StringConverter(str).to_uint(&ok);

                if (ok)
                    ret = Variant(u);
            }
            break;
        case CALI_TYPE_DOUBLE:
            ret = Variant(std::stod(str));
            break;
        case CALI_TYPE_BOOL:
            {
                bool ok = false;
                bool b  = StringConverter(str).to_bool(&ok);
                
                if (ok)
                    ret = Variant(b);
            }
            break;
        case CALI_TYPE_TYPE:
            ret = Variant(cali_string2type(str.c_str()));
            break;
        }

        return ret;
    }
    
    const Node* merge_node_record(const RecordMap& rec, IdMap& idmap) {
        cali_id_t node_id = ::id_from_rec(rec, "id");
        cali_id_t attr_id = ::map_id_from_rec(rec, "attr",   idmap);
        cali_id_t prnt_id = ::map_id_from_rec(rec, "parent", idmap);
        Variant   v_data;

        {
            auto it = rec.find("data");

            if (it != rec.end() && !it->second.empty())
                v_data = make_variant(attribute(attr_id).type(), it->second.front());
        }

        if (node_id == CALI_INV_ID || attr_id == CALI_INV_ID || v_data.empty()) {
            Log(0).stream() << "CaliperMetadataDB::merge_node_record(): Invalid node record: " << rec << endl;
            return nullptr;
        }

        Node* parent = &m_root;

        if (prnt_id != CALI_INV_ID) {
            std::lock_guard<std::mutex>
                g(m_node_lock);
            
            assert(prnt_id < m_nodes.size());
            parent = m_nodes[prnt_id];
        }

        Node* node     = nullptr;
        bool  new_node = false;

        {
            std::lock_guard<std::mutex>
                g(m_node_lock);

            for ( node = parent->first_child(); node && !node->equals(attr_id, v_data); node = node->next_sibling() )
                ;

            if (!node) {
                node     = create_node(attr_id, v_data, parent);
                new_node = true;
            }
        }

        if (new_node && node->attribute() == Attribute::meta_attribute_keys().name_attr_id) {
            std::lock_guard<std::mutex>
                g(m_attribute_lock);
            
            m_attributes.insert(make_pair(string(node->data().to_string()), node));
        }

        if (node_id != node->id())
            idmap.insert(make_pair(node_id, node->id()));

        return node;
    }

    EntryList merge_ctx_record_to_list(const RecordMap& rec, IdMap& idmap) {
        EntryList list;

        auto r_it = rec.find("ref");

        if (r_it != rec.end())
            for (const std::string& str : r_it->second) {
                cali_id_t id = ::map_id_from_string(str, idmap);

                std::lock_guard<std::mutex>
                    g(m_node_lock);
                
                if (id < m_nodes.size())
                    list.push_back(Entry(m_nodes[id]));
            }

        auto a_it = rec.find("attr");
        auto d_it = rec.find("data");

        if (a_it != rec.end() && d_it != rec.end() && a_it->second.size() == d_it->second.size())
            for (EntryList::size_type i = 0; i < a_it->second.size(); ++i) {
                Attribute attr = attribute(::map_id_from_string(a_it->second[i], idmap));

                if (attr != Attribute::invalid)
                    list.push_back(Entry(attr, make_variant(attr.type(), d_it->second[i])));
            }
        
        return list;
    }

    RecordMap merge_ctx_record(const RecordMap& rec, IdMap& idmap) {
        RecordMap record(rec);

        for (const string& key : { "ref", "attr" }) {
            auto entry_it = record.find(key);

            if (entry_it != record.end())
                std::transform(entry_it->second.begin(), entry_it->second.end(),
                               entry_it->second.begin(),
                               [&idmap](const std::string& s) {
                                   return std::to_string(::map_id_from_string(s, idmap));
                               });
        }

        return record;
    }

    RecordMap merge(const RecordMap& rec, IdMap& idmap) {
        auto rec_name_it = rec.find("__rec");

        if (rec_name_it == rec.end() || rec_name_it->second.empty())
            return rec;

        if (rec_name_it->second.front() == "node") {
            const Node* node = merge_node_record(rec, idmap);

            if (node)
                return node->record();
        } else if (rec_name_it->second.front() == "ctx" )
            return merge_ctx_record (rec, idmap);

        return rec;
    }

    void merge(CaliperMetadataDB* db, const RecordMap& rec, IdMap& idmap, NodeProcessFn& node_fn, SnapshotProcessFn& snap_fn) {
        auto rec_name_it = rec.find("__rec");

        if (rec_name_it == rec.end() || rec_name_it->second.empty())
            return;

        if (rec_name_it->second.front() == "node") {
            const Node* node = merge_node_record(rec, idmap);

            if (node)
                node_fn(*db, node);
        } else if (rec_name_it->second.front() == "ctx" )
            snap_fn(*db, merge_ctx_record_to_list(rec, idmap));
    }

    Attribute attribute(cali_id_t id) const {
        std::lock_guard<std::mutex>
            g(m_node_lock);
        
        if (id >= m_nodes.size())
            return Attribute::invalid;

        return Attribute::make_attribute(m_nodes[id]);
    }

    Attribute attribute(const std::string& name) const {
        std::lock_guard<std::mutex>
            g(m_attribute_lock);
        
        auto it = m_attributes.find(name);

        return it == m_attributes.end() ? Attribute::invalid :
            Attribute::make_attribute(it->second);
    }

    Node* make_tree_entry(std::size_t n, const Attribute attr[], const Variant data[], Node* parent = 0) {
        Node* node = nullptr;

        if (!parent)
            parent = &m_root;

        for (size_t i = 0; i < n; ++i) {
            if (attr[i].store_as_value())
                continue;

            std::lock_guard<std::mutex>
                g(m_node_lock);
            
            for (node = parent->first_child(); node && !node->equals(attr[i].id(), data[i]); node = node->next_sibling())
                ;

            if (!node)
                node = create_node(attr[i].id(), data[i], parent);

            parent = node;
        }

        return node;
    }

    Node* make_tree_entry(std::size_t n, const Node* nodelist[], Node* parent = 0) {
        Node* node = nullptr;

        if (!parent)
            parent = &m_root;

        for (size_t i = 0; i < n; ++i) {
            std::lock_guard<std::mutex>
                g(m_node_lock);
            
            for (node = parent->first_child(); node && !node->equals(nodelist[i]->attribute(), nodelist[i]->data()); node = node->next_sibling())
                ;

            if (!node)
                node = create_node(nodelist[i]->attribute(), nodelist[i]->data(), parent);

            parent = node;
        }

        return node;
    }

    Attribute create_attribute(const std::string& name, cali_attr_type type, int prop,
                               int meta, const Attribute* meta_attr, const Variant* meta_data) {
        std::lock_guard<std::mutex>
            g(m_attribute_lock);
        
        // --- Check if attribute exists
        
        auto it = m_attributes.lower_bound(name);

        if (it != m_attributes.end() && it->first == name)
            return Attribute::make_attribute(it->second);

        // --- Create attribute
        
        Node* parent = m_type_nodes[type];

        if (meta > 0)
            parent = make_tree_entry(meta, meta_attr, meta_data, parent);

        Attribute n_attr[2] = { attribute(Attribute::meta_attribute_keys().prop_attr_id),
                                attribute(Attribute::meta_attribute_keys().name_attr_id) };
        Variant   n_data[2] = { Variant(prop),
                                Variant(name) /* FIXME: Using explicit string rep */ }; 

        Node* node = make_tree_entry(2, n_attr, n_data, parent);

        m_attributes.insert(make_pair(string(name), node));
        
        return Attribute::make_attribute(node);
    }
    
    CaliperMetadataDBImpl()
        : m_root { CALI_INV_ID, CALI_INV_ID, { } }
        {
            setup_bootstrap_nodes();
        }

    ~CaliperMetadataDBImpl() {
        for (const char* str : m_string_db)
            delete[] str;
        for (Node* n : m_nodes)
            delete n;
    }
}; // CaliperMetadataDBImpl


//
// --- Public API
//

CaliperMetadataDB::CaliperMetadataDB()
    : mP { new CaliperMetadataDBImpl }
{ }

CaliperMetadataDB::~CaliperMetadataDB()
{ } 

RecordMap
CaliperMetadataDB::merge(const RecordMap& rec, IdMap& idmap)
{
    return mP->merge(rec, idmap);
}

void 
CaliperMetadataDB::merge(const RecordMap& rec, IdMap& map, NodeProcessFn& node_fn, SnapshotProcessFn& snap_fn)
{
    mP->merge(this, rec, map, node_fn, snap_fn);
}


Node* 
CaliperMetadataDB::node(cali_id_t id) const
{
    std::lock_guard<std::mutex>
        g(mP->m_node_lock);

    return (id < mP->m_nodes.size()) ? mP->m_nodes[id] : nullptr;
}

Attribute
CaliperMetadataDB::get_attribute(cali_id_t id) const
{
    return mP->attribute(id);
}

Attribute
CaliperMetadataDB::get_attribute(const std::string& name) const
{
    return mP->attribute(name);
}

Node*
CaliperMetadataDB::make_tree_entry(size_t n, const Node* nodelist[], Node* parent)
{
    return mP->make_tree_entry(n, nodelist, parent);
}

Attribute
CaliperMetadataDB::create_attribute(const std::string& name, cali_attr_type type, int prop, 
                                    int meta, const Attribute* meta_attr, const Variant* meta_data)
{
    return mP->create_attribute(name, type, prop, meta, meta_attr, meta_data);
}
