/// @file Writer.h
/// Node and Attribute writer classes abstract interfaces

#ifndef CALI_WRITER_H
#define CALI_WRITER_H

#include "cali_types.h"

#include <tuple>


namespace cali
{

class Attribute;
class NodeQuery;

class NodeWriter
{

public:

    virtual void write(const NodeQuery&) = 0;
};

class AttributeWriter
{

public:

    virtual void write(const Attribute&) = 0;
};

}

#endif 
