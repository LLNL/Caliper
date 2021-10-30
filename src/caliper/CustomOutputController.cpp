// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "CustomOutputController.h"

#include "caliper/common/OutputStream.h"

using namespace cali;
using namespace cali::internal;

//
// --- CustomOutputController::Comm
//

CustomOutputController::Comm::~Comm()
{
}

int CustomOutputController::Comm::rank() const
{
    return 0;
}

int CustomOutputController::Comm::bcast_int(int val) const 
{
    return val;
}

std::string CustomOutputController::Comm::bcast_str(const std::string& val) const
{
    return val;
}

void CustomOutputController::Comm::cross_aggregate(CaliperMetadataDB& db, Aggregator& agg) const
{
    // no-op
}

CustomOutputController::CustomOutputController(const char* name, int flags, const config_map_t& initial_cfg)
    : ChannelController(name, flags, initial_cfg)
{ }
