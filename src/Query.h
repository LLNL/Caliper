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

    virtual ctx_id_t      attribute() const = 0;
    virtual std::string   attribute_name() const = 0;
    virtual ctx_attr_type type() const = 0;

    virtual std::size_t   size() const = 0;
    virtual const void*   data() const = 0;

    virtual bool          valid() const = 0;
};

std::ostream& operator << (std::ostream& os, const Query& entry);


// --- NodeQuery

class NodeQuery : public Query
{
public:

    virtual ctx_id_t     id() const = 0;
    virtual ctx_id_t     parent() const = 0;
    virtual ctx_id_t     first_child() const = 0;
    virtual ctx_id_t     next_sibling() const = 0;
};

}

#endif // CALI_QUERY_H
