// Copyright (c) 2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "OutputCommMpi.h"

#include "caliper/cali-mpi.h"

#include "caliper/common/Log.h"

#include <vector>

using namespace cali;

struct OutputCommMpi::OutputCommMpiImpl
{
    MPI_Comm comm { MPI_COMM_NULL };
    int      rank { 0 };

    bool active() const {
        return comm != MPI_COMM_NULL;
    }

    OutputCommMpiImpl(MPI_Comm comm_)
    {
        if (comm_ != MPI_COMM_NULL) {
            MPI_Comm_dup(comm_, &comm);
            MPI_Comm_rank(comm, &rank);
        }
    }

    ~OutputCommMpiImpl()
    {
        if (comm != MPI_COMM_NULL)
            MPI_Comm_free(&comm);
    }
};

OutputCommMpi::OutputCommMpi()
{
    int initialized = 0;
    int finalized = 0;

    MPI_Initialized(&initialized);
    MPI_Finalized(&finalized);

    if (finalized)
        Log(1).stream() << "OutputCommMpi: MPI is finalized" << std::endl;
    else if (!initialized)
        Log(1).stream() << "OutputCommMpi: MPI not initialized" << std::endl;
    else if (Log::verbosity() >= 2)
        Log(2).stream() << "OutputCommMpi: MPI is available" << std::endl;

    MPI_Comm comm = (initialized && !finalized ? MPI_COMM_WORLD : MPI_COMM_NULL);

    mP.reset(new OutputCommMpiImpl(comm));
}

OutputCommMpi::OutputCommMpi(MPI_Comm comm)
    : mP { new OutputCommMpiImpl(comm) }
{ }

OutputCommMpi::~OutputCommMpi()
{ }

int OutputCommMpi::rank() const
{
    return mP->rank;
}

int OutputCommMpi::bcast_int(int val) const
{
    if (mP->active())
        MPI_Bcast(&val, 1, MPI_INT, 0, mP->comm);

    return val;
}

std::string OutputCommMpi::bcast_str(const std::string& str) const
{
    if (mP->active()) {
        unsigned len = str.length();

        MPI_Bcast(&len, 1, MPI_UNSIGNED, 0, mP->comm);

        std::vector<char> buf(len, '\0');
        std::copy(str.begin(), str.end(), std::begin(buf));

        MPI_Bcast(buf.data(), len, MPI_CHAR, 0, mP->comm);

        return { std::begin(buf), std::end(buf) };
    }

    return str;
}

void OutputCommMpi::cross_aggregate(CaliperMetadataDB& db, Aggregator& agg) const
{
    if (mP->active())
        aggregate_over_mpi(db, agg, mP->comm);
}
