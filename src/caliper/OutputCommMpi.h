// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file OutputCommMpi.h
/// OutputCommMpi class

#pragma once

#ifndef CALI_OUTPUT_COMM_MPI_H
#define CALI_OUTPUT_COMM_MPI_H

#include "CustomOutputController.h"

#include <mpi.h>

#include <memory>

namespace cali
{

class Aggregator;
class CaliperMetadataDB;

/// \brief A CustomOutputController::Comm implementation for MPI
class OutputCommMpi : public internal::CustomOutputController::Comm
{
    struct OutputCommMpiImpl;
    std::shared_ptr<OutputCommMpiImpl> mP;

public:

    OutputCommMpi();

    OutputCommMpi(MPI_Comm comm);

    ~OutputCommMpi();

    int rank() const override;

    int         bcast_int(int val) const override;
    std::string bcast_str(const std::string& str) const override;

    void cross_aggregate(CaliperMetadataDB& db, Aggregator& agg) const override;
};

} // namespace cali

#endif
