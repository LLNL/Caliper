/// @file Reader.h
/// Definition of node and Attribute reader virtual base classes

#ifndef CALI_READER_H
#define CALI_READER_H

#include "cali_types.h"

#include "Query.h"

#include <memory>


namespace cali
{

class AttributeReader
{

public:

    struct AttributeInfo {
        ctx_id_t      id;
        std::string   name;
        ctx_attr_type type;
        int           properties;
    };

    virtual AttributeInfo read() = 0;
};


class NodeReader
{

public:

    virtual std::unique_ptr<NodeQuery> read() = 0;
};

}

#endif
