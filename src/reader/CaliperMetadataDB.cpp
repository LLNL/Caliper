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

#include <cassert>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>

using namespace cali;
using namespace std;


struct CaliperMetadataDB::CaliperMetadataDBImpl
{
    Node                      m_root;         ///< (Artificial) root node
    vector<Node*>             m_nodes;        ///< Node list
    mutable mutex             m_node_lock;

    Node*                     m_type_nodes[CALI_MAXTYPE+1] = { 0 };
    
    map<string, Node*>        m_attributes;
    mutable mutex             m_attribute_lock;
    
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

    void insert_node(const RecordMap& rec) {
        Variant id, attr, parent, data;

        struct entry_t { 
            const char* key; Variant* val; 
        } entries[] = {
                { "id",     &id     }, { "attribute", &attr }, 
                { "parent", &parent }, { "data",      &data }
        };

        for ( entry_t& e : entries ) {
            auto it = rec.find(string(e.key));
            if (it != rec.end() && !it->second.empty())
                *(e.val) = it->second.front();
        }

        if (!id || !attr || !data || id.to_id() == CALI_INV_ID)
            return;

        // FIXME: Consider need to do deep copies!

        Node* node = new Node (id.to_id(), attr.to_id(), data);

        m_node_lock.lock();
        
        // FIXME: Do some error checking here

        if (m_nodes.size() <= node->id())
            m_nodes.resize(node->id() + 1);

        m_nodes[node->id()] = node;

        if (parent && parent.to_id() < m_nodes.size())
            m_nodes[parent.to_id()]->append(node);
        else
            m_root.append(node);
        
        m_node_lock.unlock();
        
        // Is this an attribute node? if so, put it in the dict
        std::lock_guard<std::mutex>
            g_attr(m_attribute_lock);
        
        if (node->attribute() == Attribute::meta_attribute_keys().name_attr_id)
            m_attributes.insert(make_pair(string(node->data().to_string()), node));
    }

    Node* create_node(cali_id_t attr_id, const Variant& data, Node* parent) {
        // NOTE: We assume that m_node_lock is locked!
        
        Node* node = new Node(m_nodes.size(), attr_id, data);
        
        m_nodes.push_back(node);

        if (parent)
            parent->append(node);

        return node;
    }

    const Node* merge_node_record(const RecordMap& rec, IdMap& idmap) {
        Variant v_id, v_attr, v_parent, v_data;

        struct entry_t { 
            const char* key; Variant* val; 
        } entries[] = {
            { "id",     &v_id     }, { "attr", &v_attr }, 
            { "parent", &v_parent }, { "data", &v_data }
        };

        for ( entry_t& e : entries ) {
            auto it = rec.find(string(e.key));
            if (it != rec.end() && !it->second.empty())
                *(e.val) = it->second.front();
        }

        if (!v_id || !v_attr || !v_data || v_id.to_id() == CALI_INV_ID || v_attr.to_id() == CALI_INV_ID) {
            Log(1).stream() << "Invalid node record format: " << rec << endl;
            return nullptr;
        }

        auto attr_it   = idmap.find(v_attr.to_id());
        cali_id_t attr = (attr_it == idmap.end() ? v_attr.to_id() : attr_it->second);

        Node* parent = &m_root;

        if (!v_parent.empty()) {
            auto parent_it = idmap.find(v_parent.to_id());
            cali_id_t id   = (parent_it == idmap.end() ? v_parent.to_id() : parent_it->second);

            std::lock_guard<std::mutex>
                g(m_node_lock);
            
            assert(id < m_nodes.size());
            parent = m_nodes[id];
        }

        Node* node     = nullptr;
        bool  new_node = false;

        {
            std::lock_guard<std::mutex>
                g(m_node_lock);

            for ( node = parent->first_child(); node && !node->equals(attr, v_data); node = node->next_sibling() )
                ;

            if (!node) {
                node     = create_node(attr, v_data, parent);
                new_node = true;
            }
        }

        if (new_node && node->attribute() == Attribute::meta_attribute_keys().name_attr_id) {
            std::lock_guard<std::mutex>
                g(m_attribute_lock);
            
            m_attributes.insert(make_pair(string(node->data().to_string()), node));
        }

        if (v_id.to_id() != node->id())
            idmap.insert(make_pair(v_id.to_id(), node->id()));

        return node;
    }

    EntryList merge_ctx_record_to_list(const RecordMap& rec, IdMap& idmap) {
        EntryList list;

        auto r_it = rec.find("ref");

        if (r_it != rec.end())
            for (const Variant& v : r_it->second) {
                cali_id_t id  = v.to_id();
                auto idmap_it = idmap.find(id);

                if (idmap_it != idmap.end())
                    id = idmap_it->second;

                std::lock_guard<std::mutex>
                    g(m_node_lock);
                
                if (id < m_nodes.size())
                    list.push_back(Entry(m_nodes[id]));
            }

        auto a_it = rec.find("attr");
        auto d_it = rec.find("data");

        if (a_it != rec.end() && d_it != rec.end() && a_it->second.size() == d_it->second.size())
            for (EntryList::size_type i = 0; i < a_it->second.size(); ++i) {
                cali_id_t id  = a_it->second[i].to_id();
                auto idmap_it = idmap.find(id);

                if (idmap_it != idmap.end())
                    id = idmap_it->second;

                list.push_back(Entry(attribute(id), d_it->second[i]));
            }

        return list;
    }

    RecordMap merge_ctx_record(const RecordMap& rec, IdMap& idmap) {
        RecordMap record(rec);

        for (const string& entry : { "ref", "attr" }) {
            auto entry_it = record.find(entry);

            if (entry_it != record.end())
                for (Variant& elem : entry_it->second) {
                    auto id_it = idmap.find(elem.to_id());
                    if (id_it != idmap.end())
                        elem = Variant(id_it->second);
                }
        }

        return record;
    }

    RecordMap merge(const RecordMap& rec, IdMap& idmap) {
        auto rec_name_it = rec.find("__rec");

        if (rec_name_it == rec.end() || rec_name_it->second.empty())
            return rec;

        if (rec_name_it->second.front().to_string() == "node") {
            const Node* node = merge_node_record(rec, idmap);

            if (node)
                return node->record();
        } else if (rec_name_it->second.front().to_string() == "ctx" )
            return merge_ctx_record (rec, idmap);

        return rec;
    }

    void merge(CaliperMetadataDB* db, const RecordMap& rec, IdMap& idmap, NodeProcessFn& node_fn, SnapshotProcessFn& snap_fn) {
        auto rec_name_it = rec.find("__rec");

        if (rec_name_it == rec.end() || rec_name_it->second.empty())
            return;

        if (rec_name_it->second.front().to_string() == "node") {
            const Node* node = merge_node_record(rec, idmap);

            if (node)
                node_fn(*db, node);
        } else if (rec_name_it->second.front().to_string() == "ctx" )
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
    
    bool read(const char* filename) {
        for (Node* n : m_nodes)
            delete n;

        m_nodes.clear();

        CsvReader reader(filename);

        bool ret = 
            reader.read(std::bind(&CaliperMetadataDBImpl::insert_node, this, std::placeholders::_1));

        cerr << "Read " << m_nodes.size() << " nodes" << endl;

        return ret;
    }

    Node* make_entry(size_t n, const Attribute* attr, const Variant* value, Node* parent = 0) {
        Node* node = nullptr;

        if (!parent)
            parent = &m_root;

        for (size_t i = 0; i < n; ++i) {
            if (attr[i].store_as_value())
                continue;

            std::lock_guard<std::mutex>
                g(m_node_lock);
            
            for (node = parent->first_child(); node && !node->equals(attr[i].id(), value[i]); node = node->next_sibling())
                ;

            if (!node)
                node = create_node(attr[i].id(), value[i], parent);

            parent = node;
        }

        return node;
    }

    Attribute create_attribute(const std::string& name, cali_attr_type type, int prop) {
        std::lock_guard<std::mutex>
            g(m_attribute_lock);
        
        // --- Check if attribute exists
        
        auto it = m_attributes.lower_bound(name);

        if (it != m_attributes.end() && it->first == name)
            return Attribute::make_attribute(it->second);

        // --- Create attribute
        
        Node*     typenode  = m_type_nodes[type];
        Attribute n_attr[2] = { attribute(Attribute::meta_attribute_keys().prop_attr_id),
                                attribute(Attribute::meta_attribute_keys().name_attr_id) };
        Variant   n_data[2] = { Variant(prop),
                                Variant(name) /* FIXME: Using explicit string rep */ }; 

        Node* node = make_entry(2, n_attr, n_data, typenode);

        m_attributes.insert(make_pair(string(name), node));
        
        return Attribute::make_attribute(node);
    }
    
    CaliperMetadataDBImpl()
        : m_root { CALI_INV_ID, CALI_INV_ID, { } }
        {
            setup_bootstrap_nodes();
        }

    ~CaliperMetadataDBImpl() {
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

bool
CaliperMetadataDB::read(const char* filename)
{
    return mP->read(filename);
}

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


const Node* 
CaliperMetadataDB::node(cali_id_t id) const
{
    std::lock_guard<std::mutex>
        g(mP->m_node_lock);

    return (id < mP->m_nodes.size()) ? mP->m_nodes[id] : nullptr;
}

Attribute
CaliperMetadataDB::attribute(cali_id_t id) const
{
    return mP->attribute(id);
}

Attribute
CaliperMetadataDB::attribute(const std::string& name) const
{
    return mP->attribute(name);
}

const Node*
CaliperMetadataDB::make_entry(size_t n, const Attribute* attr, const Variant* value)
{
    return mP->make_entry(n, attr, value);
}

Attribute
CaliperMetadataDB::create_attribute(const std::string& name, cali_attr_type type, int prop)
{
    return mP->create_attribute(name, type, prop);
}

Node* 
CaliperMetadataDB::mutable_node(cali_id_t id) 
{
    std::lock_guard<std::mutex>
        g(mP->m_node_lock);
    
    return (id < mP->m_nodes.size()) ? mP->m_nodes[id] : nullptr;
}
