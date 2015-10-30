/// @file Attribute.cpp
/// Attribute class implementation

#include "Attribute.h"

#include "Node.h"

using namespace cali;
using namespace std;

/*
namespace 
{
    vector<string> split(const std::string& list, char sep)
    {
        vector<string> vec;
        string str;

        for (auto it = list.begin(); it != list.end(); ++it) {
            if (*it == sep) {
                vec.push_back(str);
                str.clear();
            } else if (!isspace(*it))
                str.push_back(*it);
        }

        vec.push_back(str);

        return vec;
    }

    int read_properties(const std::string& str)
    {
        const map<string, cali_attr_properties> propmap = { 
            { "value",         CALI_ATTR_ASVALUE       }, 
            { "nomerge",       CALI_ATTR_NOMERGE       }, 
            { "scope_process", CALI_ATTR_SCOPE_PROCESS },
            { "scope_thread",  CALI_ATTR_SCOPE_THREAD  },
            { "scope_task",    CALI_ATTR_SCOPE_TASK    } };

        int prop = CALI_ATTR_DEFAULT;

        for (const string &s : split(str, ':')) { 
            auto it = propmap.find(s);

            if (it != propmap.end())
                prop |= it->second;
        }

        return prop;
    }

    string write_properties(int properties) 
    {
        const struct property_tbl_entry {
            cali_attr_properties p; int mask; const char* str;
        } property_tbl[] = { 
            { CALI_ATTR_ASVALUE,       CALI_ATTR_ASVALUE,     "value"          }, 
            { CALI_ATTR_NOMERGE,       CALI_ATTR_NOMERGE,     "nomerge"        }, 
            { CALI_ATTR_SCOPE_PROCESS, CALI_ATTR_SCOPE_MASK,  "scope_process"  },
            { CALI_ATTR_SCOPE_TASK,    CALI_ATTR_SCOPE_MASK,  "scope_task"     },
            { CALI_ATTR_SCOPE_THREAD,  CALI_ATTR_SCOPE_MASK,  "scope_thread"   },
            { CALI_ATTR_SKIP_EVENTS,   CALI_ATTR_SKIP_EVENTS, "skip_events"    },
            { CALI_ATTR_HIDDEN,        CALI_ATTR_HIDDEN,      "hidden"         }
        };

        int    count = 0;
        string str;

        for ( property_tbl_entry e : property_tbl )
            if (e.p == (e.mask & properties))
                str.append(count++ > 0 ? ":" : "").append(e.str);

        return str;
    }
}
*/

Attribute 
Attribute::make_attribute(const Node* node, const AttributeKeys& keys)
{
    // sanity check: make sure we have the necessary attributes (name and type)

    // Given node must be attribute name 

    if (!node || node->attribute() == CALI_INV_ID || node->attribute() != keys.name_attr_id)
        return Attribute::invalid;

    // Find type attribute
    for (const Node* p = node; p && p->attribute() != CALI_INV_ID; p = p->parent()) 
        if (p->attribute() == keys.type_attr_id)
            return Attribute(node, keys);

    return Attribute::invalid;
}

cali_id_t
Attribute::id() const
{ 
    return m_node ? m_node->id() : CALI_INV_ID;
}

std::string
Attribute::name() const 
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == m_keys.name_attr_id)
            return node->data().to_string();

    return std::string();
}

cali_attr_type
Attribute::type() const 
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == m_keys.type_attr_id)
            return node->data().to_attr_type();

    return CALI_TYPE_INV;
}

int
Attribute::properties() const 
{
    for (const Node* node = m_node; node && node->attribute() != CALI_INV_ID; node = node->parent())
        if (node->attribute() == m_keys.prop_attr_id)
            return node->data().to_int();

    return CALI_ATTR_DEFAULT;
}

const Attribute::AttributeKeys Attribute::AttributeKeys::invalid { CALI_INV_ID, CALI_INV_ID, CALI_INV_ID };

const Attribute Attribute::invalid { 0, AttributeKeys::invalid };
