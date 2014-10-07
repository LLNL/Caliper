/// @file CsvWriter.cpp
/// CsvWriter implementation

#include "CsvWriter.h"

#include "Attribute.h"
#include "Query.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>


using namespace cali;
using namespace std;


namespace
{

class CsvSpec
{
    std::string m_sep       { ","    }; ///< separator character
    std::string m_delim     { ':'    }; ///< delimiter
    char        m_esc       { '\\'   }; ///< escape character
    std::string m_esc_chars { "\\\"" }; ///< characters that need to be escaped


    // --- helpers

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

    void write_data(ostream& os, ctx_attr_type type, const void* data, size_t size) const
    {
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


public:

    // --- interface

    void write_node(ostream& os, const NodeQuery& q) const {
        // tree info
        for ( ctx_id_t i : { q.id(), q.parent(), q.first_child(), q.next_sibling() } ) {
            if (i != CTX_INV_ID)
                os << i;

            os << m_sep;
        }

        // attribute / type info
        os << q.attribute() << m_sep;

        write_type(os, q.type());
        os << m_sep;

        // data
        write_data(os, q.type(), q.data(), q.size());
        os << endl;
    }

    void write_attr(ostream& os, const Attribute& attr) const {
        os << attr.id() << m_sep;

        write_type(os, attr.type());
        os << m_sep;

        write_properties(os, attr.properties());
        os << m_sep;

        write_string(os, attr.name());
        os << endl;
    }
};

static const CsvSpec CaliperCsvSpec;

} // anonymous namespace


//
// --- CsvWriter interface
// 

void CsvWriter::write_attribute(ostream& os, const Attribute& a)
{
    ::CaliperCsvSpec.write_attr(os, a);
}

void CsvWriter::write_node(ostream& os, const NodeQuery& q)
{
    ::CaliperCsvSpec.write_node(os, q);
}
