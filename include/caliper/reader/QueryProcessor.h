// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file QueryProcessor.h
/// QueryProcessor class

#pragma once

#include "QuerySpec.h"
#include "RecordProcessor.h"

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;
class OutputStream;

/// \brief Execute a given query (filter, aggregation, and output formatting)
///   on a series of snapshot records.
class QueryProcessor
{
    struct QueryProcessorImpl;
    std::shared_ptr<QueryProcessorImpl> mP;
    
public:

    QueryProcessor(const QuerySpec&, OutputStream& stream);

    ~QueryProcessor();

    void process_record(CaliperMetadataAccessInterface&, const EntryList&);

    void flush(CaliperMetadataAccessInterface&);

    void operator()(CaliperMetadataAccessInterface& db, const EntryList& rec) {
        process_record(db, rec);
    }
};

}
