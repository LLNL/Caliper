/// @file TextNodeWriter.cpp
/// TextNodeWriter implementation

#include "TextNodeWriter.h"

#include "Query.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>


using namespace cali;
using namespace std;


struct TextNodeWriter::TextNodeWriterImpl
{
    ostream&    m_os;

    std::string m_sep { "," };          ///< separator character
    char        m_esc { '\\' };         ///< escape character
    std::string m_esc_chars { "\\\"" }; ///< characters that need to be escaped


    // --- constructor

    TextNodeWriterImpl(ostream& os) 
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

    void write_data(ctx_attr_type type, const void* data, size_t size) 
    {
        auto saveflags = m_os.flags();

        if (!data)
            type = CTX_TYPE_INV;

        switch (type) {
        case CTX_TYPE_USR:
            m_os << ios_base::hex;// << setw(2) << setfill(0);
            copy(static_cast<const char*>(data), static_cast<const char*>(data) + size,
                 ostream_iterator<char>(m_os, ""));
            break;

        case CTX_TYPE_STRING:
            write_string(string(static_cast<const char*>(data), size));
            break;
            
        case CTX_TYPE_ADDR:
            m_os << ios_base::hex;
        case CTX_TYPE_INT:
            if (size >= sizeof(uint64_t))
                m_os << *static_cast<const uint64_t*>(data);
            break;

        case CTX_TYPE_DOUBLE:
            if (size >= sizeof(double))
                m_os << *static_cast<const double*>(data);
            break;
            
        default: 
            m_os << "INVALID";
        }

        m_os.flags(saveflags);
    }

    // --- interface

    void write(const NodeQuery& q) {
        // tree info

        for ( ctx_id_t i : { q.id(), q.parent(), q.first_child(), q.next_sibling() } ) {
            if (i != CTX_INV_ID)
                m_os << i;

            m_os << m_sep;
        }

        m_os << q.attribute() << m_sep;

        write_type(q.type());
        m_os << m_sep;

        write_data(q.type(), q.data(), q.size());
        m_os << endl;
    }
};


//
// --- TextNodeWriter interface
// 

TextNodeWriter::TextNodeWriter(std::ostream& os)
    : mP { new TextNodeWriterImpl(os) } 
{ }

TextNodeWriter::~TextNodeWriter()
{
    mP.reset();
}

void TextNodeWriter::write(const NodeQuery& q)
{
    mP->write(q);
}
