/// @file CsvSpec.cpp
/// CsvSpec implementation

#include "CsvSpec.h"

#include "Attribute.h"
#include "Query.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>

using namespace cali;
using namespace std;

namespace 
{

struct CsvSpecImpl
{
    std::string m_sep       { ","    }; ///< separator character
    std::string m_delim     { ':'    }; ///< delimiter
    char        m_esc       { '\\'   }; ///< escape character
    std::string m_esc_chars { "\\\"" }; ///< characters that need to be escaped

    // --- write interface

    void write_string(ostream& os, const char* str, size_t size) const {
        os << '"';

        for (size_t i = 0; i < size; ++i) {
            if (m_esc_chars.find(str[i]) != string::npos)
                os << m_esc;
            os << str[i];
        }

        os << '"';
    }

    void write_string(ostream& os, const string& str) const {
        write_string(os, str.data(), str.size());
    }

    void write_type(ostream& os, ctx_attr_type type) const {
        const char* attr_type_string[] = { "usr", "int", "string", "addr", "double" };

        os << (type > 0 && type <= CTX_TYPE_DOUBLE ? attr_type_string[type] : "INVALID");
    }

    void write_data(ostream& os, ctx_attr_type type, const void* data, size_t size) const {
        auto saveflags = os.flags();

        if (!data)
            type = CTX_TYPE_INV;

        switch (type) {
        case CTX_TYPE_USR:
            os << ios_base::hex;// << setw(2) << setfill(0);
            copy(static_cast<const char*>(data), static_cast<const char*>(data) + size,
                 ostream_iterator<char>(os, ""));
            break;

        case CTX_TYPE_STRING:
            write_string(os, string(static_cast<const char*>(data), size));
            break;
            
        case CTX_TYPE_ADDR:
            os << ios_base::hex;
        case CTX_TYPE_INT:
            if (size >= sizeof(uint64_t))
                os << *static_cast<const uint64_t*>(data);
            break;

        case CTX_TYPE_DOUBLE:
            if (size >= sizeof(double))
                os << *static_cast<const double*>(data);
            break;
            
        default: 
            os << "INVALID";
        }

        os.flags(saveflags);
    }

    void write_properties(ostream& os, int properties) const {
        const struct property_tbl_entry {
            ctx_attr_properties p; const char* str;
        } property_tbl[] = { 
            { CTX_ATTR_ASVALUE, "value"   }, 
            { CTX_ATTR_NOMERGE, "nomerge" }, 
            { CTX_ATTR_GLOBAL,  "global"  }
        };

        int count = 0;

        for ( property_tbl_entry e : property_tbl )
            if (e.p & properties)
                os << (count++ > 0 ? m_delim : "") << e.str;
    }


    // --- read interface

    std::string read_next(istream& is) const {
        string out;

        for (char c = is.get(); is && c != m_sep[0]; c = is.get())
            if (c == m_esc)
                out.push_back(is.get());
            else if (c != '"')
                out.push_back(c);

        return out;
    }

    int read_properties(const std::string& str) const {
        const map<string, ctx_attr_properties> propmap = { 
            { "value",   CTX_ATTR_ASVALUE }, 
            { "nomerge", CTX_ATTR_NOMERGE }, 
            { "global",  CTX_ATTR_GLOBAL  } };

        int prop = CTX_ATTR_DEFAULT;

        for (string::size_type n = 0, end = 0; n < str.size(); n = end + (end == string::npos ? 0 : 1)) { 
            end = str.find(m_delim);

            auto it = propmap.find(str.substr(n, end));

            if (it != propmap.end())
                prop |= it->second;
        }

        return prop;
    }

    ctx_attr_type read_type(const std::string& str) const {
        const map<string, ctx_attr_type> typemap = { 
            { "usr",    CTX_TYPE_USR    },
            { "int",    CTX_TYPE_INT    }, 
            { "string", CTX_TYPE_STRING },
            { "addr",   CTX_TYPE_ADDR   },
            { "double", CTX_TYPE_DOUBLE } };

        auto it = typemap.find(str);

        return it == typemap.end() ? CTX_TYPE_INV : it->second;
    }

    vector<char> read_data(const std::string& str, ctx_attr_type type) const {
        vector<char> data;
        return data;
    }
}; // struct CsvSpecImpl

const CsvSpecImpl CaliperCsvSpec;

} // namespace


//
// -- public interface
//    

void
CsvSpec::write_attribute(ostream& os, const Attribute& attr)
{
    os << attr.id() << CaliperCsvSpec.m_sep;

    CaliperCsvSpec.write_type(os, attr.type());
    os << CaliperCsvSpec.m_sep;

    CaliperCsvSpec.write_properties(os, attr.properties());
    os << CaliperCsvSpec.m_sep;

    CaliperCsvSpec.write_string(os, attr.name());
    os << endl;
}

void
CsvSpec::write_node(ostream& os, const NodeQuery& q)
{
    // tree info
    for ( ctx_id_t i : { q.id(), q.parent(), q.first_child(), q.next_sibling() } ) {
        if (i != CTX_INV_ID)
            os << i;

        os << CaliperCsvSpec.m_sep;
    }

    // attribute / type info
    os << q.attribute() << CaliperCsvSpec.m_sep;

    CaliperCsvSpec.write_type(os, q.type());
    os << CaliperCsvSpec.m_sep;

    // data
    CaliperCsvSpec.write_data(os, q.type(), q.data(), q.size());
    os << endl;
}

