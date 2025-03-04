// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

// The mpiflush service. Triggers flush_and_write() at MPI_Finalize().

#include "MpiEvents.h"

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"

#include <mpi.h>

using namespace cali;

namespace
{

void mpiflush_init(Caliper* c, Channel* channel)
{
    mpiwrap_get_events(channel)->mpi_finalize_evt.connect([](Caliper* c, Channel* channel) {
        c->flush_and_write(channel, SnapshotView());
    });

    Log(1).stream() << channel->name() << ": Registered mpiflush service" << std::endl;
}

} // namespace

namespace cali
{

CaliperService mpiflush_service = { "mpiflush", ::mpiflush_init };

}
