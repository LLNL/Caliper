/// @file Writer.h
/// Node and Attribute writer classes abstract interfaces

#ifndef CALI_WRITER_H
#define CALI_WRITER_H


namespace cali
{

class Attribute;
class NodeQuery;

class Writer
{

public:

    virtual void write(const NodeQuery&) = 0;
};

}

#endif 
