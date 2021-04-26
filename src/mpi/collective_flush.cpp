// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/Caliper.h"

#include "caliper/common/OutputStream.h"

#include "caliper/reader/Aggregator.h"
#include "caliper/reader/CaliperMetadataDB.h"
#include "caliper/reader/CalQLParser.h"
#include "caliper/reader/FormatProcessor.h"
#include "caliper/reader/Preprocessor.h"
#include "caliper/reader/QuerySpec.h"
#include "caliper/reader/RecordProcessor.h"
#include "caliper/reader/RecordSelector.h"

#include "caliper/cali-mpi.h"

#include <mpi.h>

using namespace cali;

namespace cali
{

void
collective_flush(OutputStream& stream, Caliper& c, Channel& channel, const SnapshotRecord* flush_info, const QuerySpec& local_query, const QuerySpec& cross_query, MPI_Comm comm)
{
    CaliperMetadataDB db;

    db.add_attribute_aliases(cross_query.aliases);
    db.add_attribute_units(cross_query.units);

    Aggregator        cross_agg(cross_query);
    Aggregator        local_agg(local_query);

    Preprocessor      cross_pp(cross_query);
    Preprocessor      local_pp(local_query);

    RecordSelector    cross_filter(cross_query);
    RecordSelector    local_filter(local_query);

    // flush this rank's caliper data into local aggregator
    c.flush(&channel, flush_info, [&db,&local_agg,&local_pp,&local_filter](CaliperMetadataAccessInterface& in_db, const std::vector<Entry>& rec){
            EntryList mrec = local_pp.process(db, db.merge_snapshot(in_db, rec));

            if (local_filter.pass(db, mrec))
                local_agg.add(db, mrec);
        });

    // flush local aggregator results into cross-process aggregator
    local_agg.flush(db, [&cross_agg,&cross_pp,&cross_filter](CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec){
            EntryList mrec = cross_pp.process(db, rec);

            if (cross_filter.pass(db, mrec))
                cross_agg.add(db, mrec);
        });

    int rank = 0;

    // cross-process aggregation, if we have MPI
    if (comm != MPI_COMM_NULL) {
        MPI_Comm_rank(comm, &rank);

        // do the global cross-process aggregation:
        //   aggregate_over_mpi() does all the magic
        aggregate_over_mpi(db, cross_agg, comm);
    }

    // rank 0's aggregator contains the global result:
    //   create a formatter and print it out
    if (rank == 0) {
        // import globals from runtime Caliper object
        db.import_globals(c, c.get_globals(&channel));

        QuerySpec spec = cross_query;

        // set default formatter to table if it hasn't been set
        if (spec.format.opt == QuerySpec::FormatSpec::Default)
            spec.format = CalQLParser("format table").spec().format;

        FormatProcessor formatter(spec, stream);

        cross_agg.flush(db, formatter);
        formatter.flush(db);
    }
}

} // namespace cali
