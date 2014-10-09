///@file Query.cpp
/// Query interface implementation

#include "Query.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <sstream>

using namespace cali;
using namespace std;

namespace
{
    void print_data(ostream& os, ctx_attr_type type, const void* data, size_t size) 
    {
        auto saveflags = os.flags();

        if (!data)
            type = CTX_TYPE_INV;

        switch (type) {
        case CTX_TYPE_USR:
            os << ios_base::hex;
        case CTX_TYPE_STRING:
            copy(static_cast<const char*>(data), static_cast<const char*>(data) + size,
                 ostream_iterator<char>(os, ""));
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
}

ostream& cali::operator << (ostream& os, const Query& q)
{
    if (!q.valid())
        os << "{ INVALID }";
    else {
        const char* attr_type_string[] = { "usr", "int", "string", "addr", "double" };
        ctx_attr_type type     = q.type();
        const char*   type_str = type >= 0 && type <= CTX_TYPE_DOUBLE ? attr_type_string[type] : "INVALID";

        os << "{ Attr = " << q.attribute_name() 
           << ", Type = " << type_str
           << ", Size = " << q.size()
           << ", Data = ";
        ::print_data(os, type, q.data(), q.size());
        os << " }";
    }

    return os;
}
