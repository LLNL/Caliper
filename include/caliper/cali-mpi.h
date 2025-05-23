/* Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

/**
 * \file cali-mpi.h
 * \brief Miscellaneous MPI-dependent functionality
 */

#pragma once

#include <mpi.h>

#ifdef __cplusplus

#include "caliper/SnapshotRecord.h"

namespace cali
{

class Aggregator;
class Caliper;
class CaliperMetadataDB;
class OutputStream;

struct ChannelBody;
struct QuerySpec;

/**
 * \brief Perform cross-process aggregation over MPI
 *
 * Aggregates snapshot records across MPI communicator \a comm. Each
 * rank provides a local aggregation database and configuration
 * (aggregation operators and aggregation key) in Aggregator \a a. The
 * aggregation configuration should be identical on each process.
 * When the operation completes, the result will be in Aggregator \a a
 * on rank 0 of \a comm.
 *
 * This function is effectively a blocking collective operation over
 * \a comm with the usual MPI collective semantics.
 *
 * \param db   Metadata information for \a a. The metadata database
 *    may be modified during the operation.
 * \param a    Provides the aggregation configuration and local input
 *    records, and receives the result on rank 0. Other ranks may
 *    receive partial results.
 * \param comm MPI communicator.
 *
 * \ingroup ReaderAPI
 */

void aggregate_over_mpi(CaliperMetadataDB& db, Aggregator& a, MPI_Comm comm);

void collective_flush(
    OutputStream&    stream,
    Caliper&         c,
    ChannelBody*     chB,
    SnapshotView     flush_info,
    const QuerySpec& local_query,
    const QuerySpec& cross_query,
    MPI_Comm         comm
);

} /* namespace cali */

extern "C"
{
#endif

/*
 * C definitions
 */

/**
 * \brief Initialize caliper-mpi library (obsolete)
 *
 * This function is obsolete and only provided for backward compatibility.
 * It does nothing.
 */

void cali_mpi_init();

#ifdef __cplusplus
} /* extern "C" */
#endif
