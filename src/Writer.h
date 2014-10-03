/// @file Writer.h
/// Node and Attribute writer classes abstract interfaces

#ifndef CALI_WRITER_H
#define CALI_WRITER_H

#include "cali_types.h"

#include <tuple>


namespace cali
{

class Attribute;
class Query;


class NodeWriter
{

public:

    struct NodeInfo {
        ctx_id_t id;
        ctx_id_t parent;
        ctx_id_t first_child;
        ctx_id_t next_sibling;
    };

    virtual void write(const NodeInfo&, const Query&) = 0;
};

class AttributeWriter
{

public:

    virtual void write(const Attribute&) = 0;
};

}

#endif 
