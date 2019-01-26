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

// CaliperMetadataDB class definition

#include "caliper/reader/CaliperMetadataDB.h"

#include "caliper/common/csv/CsvReader.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/RecordMap.h"
#include "caliper/common/StringConverter.h"

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
    map_id(cali_id_t id, const IdMap& idmap) {
        auto it = idmap.find(id);
        return it == idmap.end() ? id : it->second;
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
    mutable mutex             m_string_db_lock;

    vector<Entry>             m_globals;
    mutex                     m_globals_lock;
    
    inline Node* node(cali_id_t id) const {
        std::lock_guard<std::mutex>
            g(m_node_lock);

        return id < m_nodes.size() ? m_nodes[id] : nullptr;
    }

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

    /// \brief Make string variant from string database 
    Variant make_string_variant(const char* str, size_t len) {
        std::lock_guard<std::mutex>
            g(m_string_db_lock);
                
        auto it = std::lower_bound(m_string_db.begin(), m_string_db.end(), str,
                                   [](const char* a, const char* b) {
                                       return strcmp(a, b) < 0;
                                   });

        if (it != m_string_db.end() && strcmp(str, *it) == 0)
            return Variant(CALI_TYPE_STRING, *it, len);

        char* ptr = new char[len + 1];
        strncpy(ptr, str, len);
        ptr[len] = '\0';
        
        m_string_db.insert(it, ptr);

        return Variant(CALI_TYPE_STRING, ptr, len);        
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
            ret = make_string_variant(str.c_str(), str.size());
            break;
        default:
            ret = Variant::from_string(type, str.c_str());
        }

        return ret;
    }

    /// Merge node given by un-mapped node info from stream with given \a idmap into DB
    /// If \a v_data is a string, it must already be in the string database!
    const Node* merge_node(cali_id_t node_id, cali_id_t attr_id, cali_id_t prnt_id, const Variant& v_data) {
        if (attribute(attr_id) == Attribute::invalid)
            attr_id = CALI_INV_ID;

        if (node_id == CALI_INV_ID || attr_id == CALI_INV_ID || v_data.empty()) {
            Log(0).stream() << "CaliperMetadataDB::merge_node(): Invalid node info: "
                            << "id="       << node_id
                            << ", attr="   << attr_id 
                            << ", parent=" << prnt_id 
                            << ", value="  << v_data
                            << std::endl;
            return nullptr;
        }
        
        Node* parent = &m_root;

        if (prnt_id != CALI_INV_ID) {
            std::lock_guard<std::mutex>
                g(m_node_lock);            

            if (prnt_id >= m_nodes.size()) {
                Log(0).stream() << "CaliperMetadataDB::merge_node(): Invalid parent node " << prnt_id << " for "
                                <<  "id="       << node_id
                                << ", attr="   << attr_id 
                                << ", parent=" << prnt_id 
                                << ", value="  << v_data
                                << std::endl;
                return nullptr;
            }

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

        return node;
    }
    
    /// Merge node given by un-mapped node info from stream with given \a idmap into DB
    /// If \a v_data is a string, it must already be in the string database!
    const Node* merge_node(cali_id_t node_id, cali_id_t attr_id, cali_id_t prnt_id, const Variant& v_data, IdMap& idmap) {
        attr_id = ::map_id(attr_id, idmap);
        prnt_id = ::map_id(prnt_id, idmap);

        const Node* node = merge_node(node_id, attr_id, prnt_id, v_data);

        if (node_id != node->id())
            idmap.insert(make_pair(node_id, node->id()));

        return node;
    }

    const Node* merge_node_record(const RecordMap& rec, IdMap& idmap) {
        cali_id_t node_id = ::id_from_rec(rec, "id");
        cali_id_t attr_id = ::id_from_rec(rec, "attr");
        cali_id_t prnt_id = ::id_from_rec(rec, "parent");
        Variant   v_data;

        {
            Attribute attr = attribute(::map_id(attr_id, idmap));

            if (attr.is_hidden()) { // skip reading data from hidden entries
                v_data = Variant(CALI_TYPE_USR, nullptr, 0);
            } else {
                auto it = rec.find("data");

                if (it != rec.end() && !it->second.empty())
                    v_data = make_variant(attr.type(), it->second.front());
            }
        }

        const Node* node = merge_node(node_id, attr_id, prnt_id, v_data, idmap);
        
        if (!node) {
            Log(0).stream() << "CaliperMetadataDB::merge_node_record(): Invalid node from record: " << rec << endl;
            return nullptr;
        }

        return node;
    }

    EntryList merge_snapshot(size_t n_nodes, const cali_id_t node_ids[],
                             size_t n_imm,   const cali_id_t attr_ids[], const Variant values[],
                             const IdMap& idmap) const
    {
        EntryList list;

        for (size_t i = 0; i < n_nodes; ++i)
            list.push_back(Entry(node(::map_id(node_ids[i], idmap)))); 
        for (size_t i = 0; i < n_imm; ++i)
            list.push_back(Entry(::map_id(attr_ids[i], idmap), values[i]));

        return list;       
    }

    const Node* recursive_merge_node(const Node* node, const CaliperMetadataAccessInterface& db) {
        if (!node || node->id() == CALI_INV_ID)
            return nullptr;
        if (node->id() < 11)
            return m_nodes[node->id()];

        const Node* attr_node = 
            recursive_merge_node(db.node(node->attribute()), db);
        const Node* parent    = 
            recursive_merge_node(node->parent(), db);

        Variant v_data = node->data();

        if (v_data.type() == CALI_TYPE_STRING)
            v_data = make_string_variant(static_cast<const char*>(v_data.data()), v_data.size());

        return merge_node(node->id(), attr_node->id(), parent ? parent->id() : CALI_INV_ID, v_data);
    }

    EntryList merge_snapshot(size_t n_nodes, const Node* const* nodes,
                             size_t n_imm,   const cali_id_t attr_ids[], const Variant values[],
                             const CaliperMetadataAccessInterface& db)
    {
        EntryList list;

        for (size_t i = 0; i < n_nodes; ++i)
            list.push_back(Entry(recursive_merge_node(nodes[i], db))); 
        for (size_t i = 0; i < n_imm; ++i) 
            list.push_back(Entry(recursive_merge_node(db.node(attr_ids[i]), db)->id(), values[i]));

        return list;       
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

        if (       rec_name_it->second.front() == "node"   ) {
            const Node* node = merge_node_record(rec, idmap);

            if (node)
                return node->record();
        } else if (rec_name_it->second.front() == "ctx"    ) {
            return merge_ctx_record(rec, idmap);
        } else if (rec_name_it->second.front() == "globals") {
            EntryList list = merge_ctx_record_to_list(rec, idmap);

            std::lock_guard<std::mutex>
                g(m_globals_lock);

            m_globals = std::move(list);
        }

        return rec;
    }

    void merge(CaliperMetadataDB* db, const RecordMap& rec, IdMap& idmap, NodeProcessFn node_fn, SnapshotProcessFn snap_fn) {
        auto rec_name_it = rec.find("__rec");

        if (rec_name_it == rec.end() || rec_name_it->second.empty())
            return;

        if (       rec_name_it->second.front() == "node"   ) {
            const Node* node = merge_node_record(rec, idmap);

            if (node)
                node_fn(*db, node);
        } else if (rec_name_it->second.front() == "ctx"    ) {
            snap_fn(*db, merge_ctx_record_to_list(rec, idmap));
        } else if (rec_name_it->second.front() == "globals") {
            EntryList list = merge_ctx_record_to_list(rec, idmap);

            std::lock_guard<std::mutex>
                g(m_globals_lock);

            m_globals = std::move(list);
        }
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

    std::vector<Attribute> get_attributes() const {
        std::lock_guard<std::mutex>
            g(m_attribute_lock);

        std::vector<Attribute> ret;
        ret.reserve(m_attributes.size());

        for (auto it : m_attributes)
            ret.push_back(Attribute::make_attribute(it.second));

        return ret;
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
                                make_variant(CALI_TYPE_STRING, name) }; 

        Node* node = make_tree_entry(2, n_attr, n_data, parent);

        m_attributes.insert(make_pair(string(name), node));
        
        return Attribute::make_attribute(node);
    }

    std::vector<Entry> get_globals() {
        std::lock_guard<std::mutex>
            g(m_globals_lock);

        return m_globals;
    }

    void set_global(const Attribute& attr, const Variant& value) {
        if (!attr.is_global())
            return;

        std::lock_guard<std::mutex>
            g(m_globals_lock);
        
        if (attr.store_as_value()) {
            auto it = std::find_if(m_globals.begin(), m_globals.end(),
                                   [attr](const Entry& e) {
                                       return e.attribute() == attr.id();
                                   } );

            if (it == m_globals.end())
                m_globals.push_back(Entry(attr, value));
            else
                *it = Entry(attr, value);
        } else {
            // check if we already have this value
            if (std::find_if(m_globals.begin(), m_globals.end(),
                             [attr,value](const Entry& e) {
                                 for (const Node* node = e.node(); node; node = node->parent())
                                     if (node->attribute() == attr.id() && node->data() == value)
                                         return true;
                                 return false;
                             } ) != m_globals.end())
                return;

            auto it = std::find_if(m_globals.begin(), m_globals.end(),
                                   [](const Entry& e) {
                                       return e.is_reference();
                                   } );

            if (it == m_globals.end() || !attr.is_autocombineable())
                m_globals.push_back(Entry(make_tree_entry(1, &attr, &value)));
            else
                *it = Entry(make_tree_entry(1, &attr, &value, node(it->node()->id())));
        }
    }

    std::vector<Entry> import_globals(CaliperMetadataAccessInterface& db) {
        std::vector<Entry> import_globals = db.get_globals();

        std::lock_guard<std::mutex>
            g(m_globals_lock);

        m_globals.clear();
        
        for (const Entry& e : import_globals)
            if (e.is_reference())
                m_globals.push_back(Entry(recursive_merge_node(e.node(), db)));
            else if (e.is_immediate())
                m_globals.push_back(Entry(recursive_merge_node(db.node(e.attribute()), db)->id(), e.value()));

        return m_globals;
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
CaliperMetadataDB::merge(const RecordMap& rec, IdMap& map, NodeProcessFn node_fn, SnapshotProcessFn snap_fn)
{
    mP->merge(this, rec, map, node_fn, snap_fn);
}

const Node*
CaliperMetadataDB::merge_node(cali_id_t node_id, cali_id_t attr_id, cali_id_t prnt_id, const Variant& value, IdMap& idmap)
{
    Variant v_data = value;

    // if value is a string, find or insert it in string DB
    if (v_data.type() == CALI_TYPE_STRING)
        v_data = mP->make_string_variant(static_cast<const char*>(value.data()), value.size());
    
    return mP->merge_node(node_id, attr_id, prnt_id, v_data, idmap);
}

EntryList
CaliperMetadataDB::merge_snapshot(size_t n_nodes, const cali_id_t node_ids[], 
                                  size_t n_imm,   const cali_id_t attr_ids[], const Variant values[],
                                  const IdMap& idmap) const
{
    return mP->merge_snapshot(n_nodes, node_ids, n_imm, attr_ids, values, idmap);
}

EntryList
CaliperMetadataDB::merge_snapshot(size_t n_nodes, const Node* const* nodes, 
                                  size_t n_imm,   const cali_id_t attr_ids[], const Variant values[],
                                  const CaliperMetadataAccessInterface& db)
{
    return mP->merge_snapshot(n_nodes, nodes, n_imm, attr_ids, values, db);
}

Node* 
CaliperMetadataDB::node(cali_id_t id) const
{
    return mP->node(id);
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

std::vector<Attribute>
CaliperMetadataDB::get_attributes() const
{
    return mP->get_attributes();
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

std::vector<Entry>
CaliperMetadataDB::get_globals()
{
    return mP->get_globals();
}

void
CaliperMetadataDB::set_global(const Attribute& attr, const Variant& value)
{
    mP->set_global(attr, value);
}

std::vector<Entry>
CaliperMetadataDB::import_globals(CaliperMetadataAccessInterface& db)
{
    return mP->import_globals(db);
}
