/* *********************************************************************************************
 * Copyright (c) 2017, Lawrence Livermore National Security, LLC.  
 * Produced at the Lawrence Livermore National Laboratory.
 *
 * This file is part of Caliper.
 * Written by David Boehme, boehme3@llnl.gov.
 * LLNL-CODE-678900
 * All rights reserved.
 *
 * For details, see https://github.com/scalability-llnl/Caliper.
 * Please also see the LICENSE file for our additional BSD notice.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the disclaimer below.
 *  * Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the disclaimer (as noted below) in the documentation and/or other materials
 *    provided with the distribution.
 *  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * *********************************************************************************************/

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
class CaliperMetadataDB;

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

} /* namespace cali */

extern "C" {
#endif

/*
 * C definitions
 */

    
#ifdef __cplusplus
} /* extern "C" */
#endif
