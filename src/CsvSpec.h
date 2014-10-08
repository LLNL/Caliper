/// @file CsvSpec.h
/// CsvSpec class definition

#ifndef CALI_CSVSPEC_H
#define CALI_CSVSPEC_H

#include "cali_types.h"

#include <iostream>
#include <string>
#include <vector>

namespace cali
{

class Attribute;
class NodeQuery;

class CsvSpec
{

public:

    static void write_node(std::ostream&, const NodeQuery&);
    static void write_attribute(std::ostream&, const Attribute&);
};

} // namespace cali

#endif
