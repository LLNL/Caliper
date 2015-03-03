/// @file Attribute.cpp
/// Attribute class implementation

#include "Attribute.h"

#include <vector>

using namespace cali;
using namespace std;


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
            { CALI_ATTR_ASVALUE,       CALI_ATTR_ASVALUE,    "value"          }, 
            { CALI_ATTR_NOMERGE,       CALI_ATTR_NOMERGE,    "nomerge"        }, 
            { CALI_ATTR_SCOPE_PROCESS, CALI_ATTR_SCOPE_MASK, "scope_process"  },
            { CALI_ATTR_SCOPE_TASK,    CALI_ATTR_SCOPE_MASK, "scope_task"     },
            { CALI_ATTR_SCOPE_THREAD,  CALI_ATTR_SCOPE_MASK, "scope_thread"   }
        };

        int    count = 0;
        string str;

        for ( property_tbl_entry e : property_tbl )
            if (e.p == (e.mask & properties))
                str.append(count++ > 0 ? ":" : "").append(e.str);

        return str;
    }
}

RecordMap Attribute::record() const
{
    RecordMap recmap = { 
        { "id",         { { id()   } }      },
        { "name",       { Variant(m_name) } },
        { "type",       { { m_type } }      },
        { "properties", { }                 } };

    if (m_properties != CALI_ATTR_DEFAULT)
        recmap["properties"].push_back(Variant(::write_properties(m_properties)));

    return recmap;
}

const Attribute Attribute::invalid { CALI_INV_ID, "", CALI_TYPE_INV, CALI_ATTR_DEFAULT };
