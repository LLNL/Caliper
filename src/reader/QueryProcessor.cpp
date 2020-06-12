// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/reader/QueryProcessor.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/Preprocessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

using namespace cali;

struct QueryProcessor::QueryProcessorImpl
{
    Aggregator        aggregator;
    Preprocessor      preprocessor;
    RecordSelector    filter;
    FormatProcessor   formatter;

    bool              do_aggregate;

    void
    process_record(CaliperMetadataAccessInterface& db, const EntryList& in_rec) {
        if (filter.pass(db, in_rec)) {
            auto rec = preprocessor.process(db, in_rec);

            if (do_aggregate)
                aggregator.add(db, rec);
            else
                formatter.process_record(db, rec);
        }
    }

    void
    flush(CaliperMetadataAccessInterface& db) {
        aggregator.flush(db, formatter);
        formatter.flush(db);
    }

    QueryProcessorImpl(const QuerySpec& spec, OutputStream& stream)
        : aggregator(spec),
          preprocessor(spec),
          filter(spec),
          formatter(spec, stream)
    {
        do_aggregate = (spec.aggregation_ops.selection != QuerySpec::AggregationSelection::None);
    }
};


QueryProcessor::QueryProcessor(const QuerySpec& spec, OutputStream& stream)
    : mP(new QueryProcessorImpl(spec, stream))
{ }

QueryProcessor::~QueryProcessor()
{
    mP.reset();
}

void
QueryProcessor::process_record(CaliperMetadataAccessInterface& db, const EntryList& rec)
{

    mP->process_record(db, rec);
}

void
QueryProcessor::flush(CaliperMetadataAccessInterface& db)
{
    mP->flush(db);
}
