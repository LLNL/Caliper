// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Table.h
/// \brief Table output formatter

#ifndef CALI_TABLE_H
#define CALI_TABLE_H

#include "Formatter.h"
#include "RecordProcessor.h"

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;
struct QuerySpec;

/// \brief Print a set of snapshot records in a human-readable table
/// \ingroup ReaderAPI
    
class TableFormatter : public Formatter
{
    struct TableImpl;
    std::shared_ptr<TableImpl> mP;

public:

    TableFormatter(const std::string& fields, const std::string& sort_fields);
    TableFormatter(const QuerySpec& spec);

    ~TableFormatter();

    void process_record(CaliperMetadataAccessInterface&, const EntryList&);

    void flush(CaliperMetadataAccessInterface&, std::ostream& os);
};

} // namespace cali

#endif
