// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "CustomOutputController.h"

#include "caliper/Caliper.h"

#include "caliper/common/Log.h"
#include "caliper/common/OutputStream.h"

using namespace cali;
using namespace cali::internal;

CustomOutputController::FlushFn CustomOutputController::s_flush_fn { nullptr };

void
CustomOutputController::set_flush_fn(FlushFn flush_fn)
{
    s_flush_fn = flush_fn;
}

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

void CustomOutputController::flush()
{
    auto chn = channel();

    if (!chn)
        return;

    if (s_flush_fn) {
        (*s_flush_fn)(this);
    } else {
        Log(2).stream() << chn->name()
                        << ": CustomOutputController::flush(): using serial flush"
                        << std::endl;

        Comm comm;
        OutputStream stream;

        collective_flush(stream, comm);
    }
}

CustomOutputController::CustomOutputController(const char* name, int flags, const config_map_t& initial_cfg)
    : ChannelController(name, flags, initial_cfg)
{ }
