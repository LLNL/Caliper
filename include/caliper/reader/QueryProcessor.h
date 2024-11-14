// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file QueryProcessor.h
/// QueryProcessor class

#pragma once

#include "RecordProcessor.h"

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;
class OutputStream;
struct QuerySpec;

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

    void operator() (CaliperMetadataAccessInterface& db, const EntryList& rec) { process_record(db, rec); }
};

} // namespace cali
