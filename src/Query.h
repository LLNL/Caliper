///@file Query.h
///Query interface declaration

#ifndef CALI_QUERY_H
#define CALI_QUERY_H

#include "cali_types.h"

#include <iostream>

namespace cali
{

// --- Query API

class Query 
{
public:

    virtual std::string   attribute() const = 0;
    virtual ctx_attr_type type() const = 0;

    virtual std::size_t   size() const = 0;
    virtual const void*   data() const = 0;

    virtual bool          valid() const = 0;
};

std::ostream& operator << (std::ostream& os, const Query& entry);

}

#endif // CALI_QUERY_H
