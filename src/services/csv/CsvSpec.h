/// @file CsvSpec.h
/// CsvSpec csv I/O implementation

#ifndef CALI_CSVSPEC_H
#define CALI_CSVSPEC_H

#include <cali_types.h>

#include <RecordMap.h>

#include <iostream>
#include <string>

namespace cali
{

class CsvSpec 
{
public:

    static void      write_record(std::ostream& os, const RecordMap& record);
    static RecordMap read_record(const std::string& line);
};

} // namespace cali

#endif // CALI_CSVSPEC_H
