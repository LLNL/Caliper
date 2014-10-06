/// @file TextAttributeWriter
/// TextAttributeWriter implementation

#include "TextAttributeWriter.h"

#include "Attribute.h"

#include <iostream>

using namespace cali;
using namespace std;


struct TextAttributeWriter::TextAttributeWriterImpl 
{
    ostream&   m_os;

    std::string m_sep   { ","  };       ///< separator character
    char        m_esc   { '\\' };       ///< escape character
    std::string m_delim { ':'  };       ///< delimiter
    std::string m_esc_chars { "\\\"" }; ///< characters that need to be escaped


    // --- constructor

    TextAttributeWriterImpl(ostream& os) 
        : m_os { os }
        { } 


    // --- helpers

    void write_string(const char* str, size_t size) {
        m_os << '"';

        for (size_t i = 0; i < size; ++i) {
            if (m_esc_chars.find(str[i]) != string::npos)
                m_os << m_esc;
            m_os << str[i];
        }

        m_os << '"';
    }

    void write_string(const string& str) {
        write_string(str.data(), str.size());
    }

    void write_type(ctx_attr_type type) {
        const char* attr_type_string[] = { "usr", "int", "string", "addr", "double" };

        m_os << (type > 0 && type <= CTX_TYPE_DOUBLE ? attr_type_string[type] : "INVALID");
    }

    void write_properties(int properties) {
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
                m_os << (count++ > 0 ? m_delim : "") << e.str;
    }

    void write(const Attribute& attr) {
        m_os << attr.id() << m_sep;

        write_type(attr.type());
        m_os << m_sep;

        write_properties(attr.properties());
        m_os << m_sep;

        write_string(attr.name());
        m_os << endl;
    }
};


//
// --- TextAttributeWriter interface
// 

TextAttributeWriter::TextAttributeWriter(std::ostream& os)
    : mP { new TextAttributeWriterImpl(os) } 
{ }

TextAttributeWriter::~TextAttributeWriter()
{
    mP.reset();
}

void TextAttributeWriter::write(const Attribute& q)
{
    mP->write(q);
}
