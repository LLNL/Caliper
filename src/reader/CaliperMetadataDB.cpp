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
#include <vector>

using namespace cali;
using namespace std;


struct CaliperMetadataDB::CaliperMetadataDBImpl
{
    Node                      m_root;         ///< (Artificial) root node
    vector<Node*>             m_nodes;        ///< Node list
    map<cali_id_t, Attribute> m_attributes;   ///< Attribute cache

    cali_id_t                 m_name_attr_id = { CALI_INV_ID };
    cali_id_t                 m_prop_attr_id = { CALI_INV_ID };
    cali_id_t                 m_type_attr_id = { CALI_INV_ID };

    
    void setup_attribute_nodes(cali_id_t id, const std::string& name) {
        struct attr_t {
            const char* name; cali_id_t* id;
        } base_attributes[] = {
            { "cali.attribute.name", &m_name_attr_id },
            { "cali.attribute.prop", &m_prop_attr_id },
            { "cali.attribute.type", &m_type_attr_id }
        };

        for ( attr_t &a : base_attributes )
            if (*a.id == CALI_INV_ID && name == a.name) {
                *a.id = id; break;
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

        // FIXME: Do some error checking here

        if (m_nodes.size() <= node->id())
            m_nodes.resize(node->id() + 1);

        m_nodes[node->id()] = node;

        if (parent && parent.to_id() < m_nodes.size())
            m_nodes[parent.to_id()]->append(node);
        else
            m_root.append(node);

        // Check if this is one of the basic attribute nodes
        setup_attribute_nodes(id.to_id(), data.to_string());
    }

    Node* create_node(cali_id_t attr_id, const Variant& data, Node* parent) {
        Node* node = new Node(m_nodes.size(), attr_id, data);
        m_nodes.push_back(node);

        if (parent)
            parent->append(node);

        return node;
    }

    RecordMap merge_node_record(const RecordMap& rec, IdMap& idmap) {
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
            Log(1).stream() << "Invalid node record format" << endl;
            return rec;
        }

        auto attr_it   = idmap.find(v_attr.to_id());
        cali_id_t attr = (attr_it == idmap.end() ? v_attr.to_id() : attr_it->second);

        Node* parent = &m_root;

        if (!v_parent.empty()) {
            auto parent_it = idmap.find(v_parent.to_id());
            cali_id_t id   = (parent_it == idmap.end() ? v_parent.to_id() : parent_it->second);

            assert(id < m_nodes.size());

            parent = m_nodes[id];
        }

        Node* node = parent->first_child();

        while ( node && !node->equals(attr, v_data) )
            node = node->next_sibling();

        if (!node) {
            node = create_node(attr, v_data, parent);

            // Check if this is one of the basic attribute nodes
            setup_attribute_nodes(node->id(), v_data.to_string());
        }

        if (v_id.to_id() != node->id())
            idmap.insert(make_pair(v_id.to_id(), node->id()));

        return node->record();
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

        if      (rec_name_it->second.front().to_string() == "node")
            return merge_node_record(rec, idmap);
        else if (rec_name_it->second.front().to_string() == "ctx" )
            return merge_ctx_record (rec, idmap);

        return rec;
    }

    Attribute attribute(cali_id_t id) {
        auto it = m_attributes.find(id);

        if (it != m_attributes.end())
            return it->second;

        // Create the attribute from node

        if (id >= m_nodes.size())
            return Attribute::invalid;

        Variant name, type, prop;
        Node*   node = m_nodes[id];

        for ( ; node ; node = node->parent() ) {
            if      (node->attribute() == m_name_attr_id) 
                name = node->data();
            else if (node->attribute() == m_prop_attr_id) 
                prop = node->data();
            else if (node->attribute() == m_type_attr_id) 
                type = node->data();
        }

        if (!name || !type)
            return Attribute::invalid;

        int p = prop ? prop.to_int() : CALI_ATTR_DEFAULT;
        Attribute attr { id, name.to_string(), type.to_attr_type(), p };

        m_attributes.insert(make_pair(id, attr));

        return attr;
    }

    bool read(const char* filename) {
        for (Node* n : m_nodes)
            delete n;

        m_nodes.clear();
        m_attributes.clear();

        m_name_attr_id = CALI_INV_ID;
        m_prop_attr_id = CALI_INV_ID;
        m_type_attr_id = CALI_INV_ID;

        CsvReader reader(filename);

        bool ret = 
            reader.read(std::bind(&CaliperMetadataDBImpl::insert_node, this, std::placeholders::_1));

        if (m_name_attr_id == CALI_INV_ID || 
            m_prop_attr_id == CALI_INV_ID || 
            m_type_attr_id == CALI_INV_ID)
            ret = false;

        cerr << "Read " << m_nodes.size() << " nodes" << endl;

        return ret;
    }

    CaliperMetadataDBImpl()
        : m_root { CALI_INV_ID, CALI_INV_ID, { } }
        { }

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

const Node* 
CaliperMetadataDB::node(cali_id_t id) const 
{
    return (id < mP->m_nodes.size()) ? mP->m_nodes[id] : nullptr;
}

Attribute
CaliperMetadataDB::attribute(cali_id_t id)
{
    return mP->attribute(id);
}
