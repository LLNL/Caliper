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
            { "value",   CALI_ATTR_ASVALUE }, 
            { "nomerge", CALI_ATTR_NOMERGE }, 
            { "global",  CALI_ATTR_GLOBAL  } };

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
            cali_attr_properties p; const char* str; const char* contstr;
        } property_tbl[] = { 
            { CALI_ATTR_ASVALUE, "value"   }, 
            { CALI_ATTR_NOMERGE, "nomerge" }, 
            { CALI_ATTR_GLOBAL,  "global"  }
        };

        int    count = 0;
        string str;

        for ( property_tbl_entry e : property_tbl )
            if (e.p & properties)
                str.append(count++ > 0 ? ":" : "").append(e.str);

        return str;
    }
}

RecordMap Attribute::record() const
{
    RecordMap recmap = { 
        { "id",   { id()   } },
        { "name", { m_name } },
        { "type", { m_type } } };

    if (m_properties != CALI_ATTR_DEFAULT)
        recmap.insert(make_pair("properties", Variant(::write_properties(m_properties))));

    return recmap;
}

Attribute Attribute::read(const RecordMap& rec)
{
    auto it = rec.find("id");
    Variant v_id   = it == rec.end() ? Variant() : it->second;
    it = rec.find("name");
    Variant v_name = it == rec.end() ? Variant() : it->second;
    it = rec.find("type");
    Variant v_type = it == rec.end() ? Variant() : it->second;

    if (!v_id || !v_name || !v_type)
        return Attribute::invalid;

    it = rec.find("properties");
    Variant v_prop = it == rec.end() ? Variant() : it->second;

    int properties = v_prop ? ::read_properties(v_prop.to_string()) : CALI_ATTR_DEFAULT;

    return Attribute(v_id.to_id(), v_name.to_string(), v_type.to_attr_type(), properties);
}

const Attribute Attribute::invalid { CALI_INV_ID, "", CALI_TYPE_INV, CALI_ATTR_DEFAULT };
