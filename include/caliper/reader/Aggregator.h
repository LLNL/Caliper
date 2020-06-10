// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file Aggregator.h
/// \brief Defines the Aggregator class

#ifndef CALI_AGGREGATOR_H
#define CALI_AGGREGATOR_H

#include "QuerySpec.h"
#include "RecordProcessor.h"

#include <iostream>
#include <memory>

namespace cali
{

class CaliperMetadataAccessInterface;

/// \brief Perform aggregation operations on Caliper data
/// \ingroup ReaderAPI
    
class Aggregator 
{
    struct AggregatorImpl;
    std::shared_ptr<AggregatorImpl> mP;

public:

    Aggregator(const QuerySpec& spec);

    ~Aggregator();

    void add(CaliperMetadataAccessInterface&, const EntryList&);

    void operator()(CaliperMetadataAccessInterface& db, const EntryList& list) {
        add(db, list);
    }

    void flush(CaliperMetadataAccessInterface&, SnapshotProcessFn push);

    static const QuerySpec::FunctionSignature*
    aggregation_defs();

    static std::string
    get_aggregation_attribute_name(const QuerySpec::AggregationOp& op);
};

} // namespace cali

#endif
