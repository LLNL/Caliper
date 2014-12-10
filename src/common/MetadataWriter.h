/// @file MetadataWriter
/// MetadataWriter base class declaration

#ifndef CALI_METADATAWRITER_H
#define CALI_METADATAWRITER_H

#include <functional>

namespace cali
{

class Attribute;
class Node;

class MetadataWriter
{
public:

    virtual bool 
    write(std::function<void(std::function<void(const Node&)>)> foreach_node) = 0;
};

} // namespace cali

#endif // CALI_METADATAWRITER_H
