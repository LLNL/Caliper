/// @file Csv.h
/// Csv reader/writer class definitions

#ifndef CALI_CSVSPEC_H
#define CALI_CSVSPEC_H

#include "cali_types.h"

#include <MetadataWriter.h>

#include <iostream>
#include <functional>
#include <memory>


namespace cali
{

class Node;

class CsvWriter : public MetadataWriter
{
    struct CsvWriterImpl;

    std::unique_ptr<CsvWriterImpl> mP;

public:

    CsvWriter();
    CsvWriter(const std::string& basename);

    ~CsvWriter();

    bool write(std::function<void(std::function<void(const Node&)>)> foreach_node) override;
};

} // namespace cali

#endif
