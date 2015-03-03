/// @file CaliperMetadataDB.cpp
/// CaliperMetadataDB class definition

#include "CaliperMetadataDB.h"

#include <CsvReader.h>

#include <Node.h>
#include <RecordMap.h>

#include <iostream>
#include <map>
#include <vector>

using namespace cali;
using namespace std;

struct CaliperMetadataDB::CaliperMetadataDBImpl
{
    vector<Node*>             m_nodes;        ///< Node list
    map<cali_id_t, Attribute> m_attributes;   ///< Attribute cache

    cali_id_t                 m_name_attr_id = { CALI_INV_ID };
    cali_id_t                 m_prop_attr_id = { CALI_INV_ID };
    cali_id_t                 m_type_attr_id = { CALI_INV_ID };

    void create_node(const RecordMap& rec) {
        Variant id, attr, parent, data;

        struct entry_t { 
            const char* key; Variant* val; 
        } entries[] = {
                { "id",     &id     }, { "attribute", &attr }, 
                { "parent", &parent }, { "data",      &data }
        };

        for ( entry_t& e : entries ) {
            auto it = rec.find(string(e.key));
            if (it != rec.end())
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

        // Check if this is one of the basic attribute nodes

        struct attr_t {
            const char* name; cali_id_t* id;
        } base_attributes[] = {
            { "cali.attribute.name", &m_name_attr_id },
            { "cali.attribute.prop", &m_prop_attr_id },
            { "cali.attribute.type", &m_type_attr_id }
        };

        for ( attr_t &a : base_attributes )
            if (*a.id == CALI_INV_ID && data.to_string() == a.name) {
                *a.id = id.to_id(); break;
            }
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
        CsvReader reader(filename);

        bool ret = 
            reader.read(std::bind(&CaliperMetadataDBImpl::create_node, this, std::placeholders::_1));

        if (m_name_attr_id == CALI_INV_ID || 
            m_prop_attr_id == CALI_INV_ID || 
            m_type_attr_id == CALI_INV_ID)
            ret = false;

        cerr << "Read " << m_nodes.size() << " nodes" << endl;

        return ret;
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
