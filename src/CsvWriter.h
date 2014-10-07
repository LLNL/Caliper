/// @file CsvWriter.h
/// @CsvWriter definition

#include <iostream>

namespace cali
{

class Attribute;
class NodeQuery;


class CsvWriter
{   

public:

    static void write_node(std::ostream&, const NodeQuery&);
    static void write_attribute(std::ostream&, const Attribute&);
};

}
