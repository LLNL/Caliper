/// @file Csv.h
/// Csv reader/writer class definitions

#ifndef CALI_CSVSPEC_H
#define CALI_CSVSPEC_H

#include "cali_types.h"

#include <iostream>
#include <functional>
#include <memory>


namespace cali
{

class Attribute;
class Node;

class CsvWriter
{
    struct CsvWriterImpl;

    std::unique_ptr<CsvWriterImpl> mP;

public:

    CsvWriter();

    ~CsvWriter();

    void write(std::function<void(std::function<void(const Attribute&)>)> foreach_attr,
               std::function<void(std::function<void(const Node&)>)>      foreach_node);
};

} // namespace cali

#endif
