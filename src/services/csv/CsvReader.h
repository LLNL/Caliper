/// @file CsvReader.h
/// CsvReader class definition

#ifndef CALI_CSVREADER_H
#define CALI_CSVREADER_H

#include <RecordMap.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace cali
{

class CsvReader
{
    struct CsvReaderImpl;

    std::unique_ptr<CsvReaderImpl> mP;

public:

    CsvReader(const std::string& filename);

    ~CsvReader();

    bool read(std::function<void(const RecordMap&)>);
};

} // namespace cali

#endif
