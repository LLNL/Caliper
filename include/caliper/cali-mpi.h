/* Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 * See top-level LICENSE file for details.
 */

/**
 * \file cali-mpi.h
 * \brief Miscellaneous MPI-dependent functionality
 */

#pragma once

#include <mpi.h>

#ifdef __cplusplus

namespace cali
{

class Aggregator;
class Caliper;
class Channel;
class CaliperMetadataDB;
class OutputStream;
class QuerySpec;
class SnapshotRecord;

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

void
aggregate_over_mpi(CaliperMetadataDB& db, Aggregator& a, MPI_Comm comm);

void
collective_flush(OutputStream&    stream,
                 Caliper&         c,
                 Channel&         channel,
                 const SnapshotRecord* flush_info,
                 const QuerySpec& local_query,
                 const QuerySpec& cross_query,
                 MPI_Comm         comm);

} /* namespace cali */

extern "C" {
#endif

/*
 * C definitions
 */

/**
 * \brief Initialize caliper-mpi library.
 *   Must run before Caliper initialization.
 *
 * Normally, the Caliper mpi library is initialized through a dynamic
 * library constructor in libcaliper-mpi.so, and it is not necessary to call
 * cali_mpi_init(). However, certain linker optimizations or static linking
 * can prevent a library from being linked if a library constructor is
 * the only entry point. In this case, call cali_mpi_init() from the target
 * application before initializing Caliper. It ensures that the Caliper mpi
 * runtime library (libcaliper-mpi) is correctly linked to the application,
 * and makes the mpi-specific services visible to the Caliper initializer.
 */

void
cali_mpi_init();

#ifdef __cplusplus
} /* extern "C" */
#endif
