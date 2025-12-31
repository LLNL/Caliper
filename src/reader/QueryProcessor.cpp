// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/reader/QueryProcessor.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/Preprocessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/common/CaliperMetadataAccessInterface.h"

using namespace cali;

struct QueryProcessor::QueryProcessorImpl {
    Aggregator      aggregator;
    Preprocessor    preprocessor;
    RecordSelector  filter;
    FormatProcessor formatter;

    bool do_aggregate;
    bool do_filter;
    bool do_preprocess;

    void process_record(CaliperMetadataAccessInterface& db, const EntryList& rec)
    {
        if (!do_filter || filter.pass(db, rec)) {
            if (do_aggregate)
                aggregator.add(db, rec);
            else
                formatter.process_record(db, rec);
        }
    }

    void process_with_preprocessor(CaliperMetadataAccessInterface& db, const EntryList& in_rec)
    {
        auto rec = preprocessor.process(db, in_rec);
        process_record(db, rec);
    }

    void flush(CaliperMetadataAccessInterface& db)
    {
        aggregator.flush(db, formatter);
        formatter.flush(db);
    }

    QueryProcessorImpl(const QuerySpec& spec, OutputStream& stream)
        : aggregator(spec), preprocessor(spec), filter(spec), formatter(spec, stream)
    {
        do_aggregate = (spec.aggregate.selection != QuerySpec::AggregationSelection::None);
        do_filter = (spec.filter.selection != QuerySpec::FilterSelection::None);
        do_preprocess = !spec.preprocess_ops.empty();
    }
};

QueryProcessor::QueryProcessor(const QuerySpec& spec, OutputStream& stream) : mP(new QueryProcessorImpl(spec, stream))
{}

void QueryProcessor::process_record(CaliperMetadataAccessInterface& db, const EntryList& rec)
{
    if (mP->do_preprocess)
        mP->process_with_preprocessor(db, rec);
    else
        mP->process_record(db, rec);
}

void QueryProcessor::flush(CaliperMetadataAccessInterface& db)
{
    mP->flush(db);
}
