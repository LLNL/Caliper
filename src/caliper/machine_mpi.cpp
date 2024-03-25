// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "machine.h"

#include "caliper/common/Log.h"

#include <mpi.h>

#include <unistd.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{

int get_rank_for_hash(uint64_t hash, MPI_Comm comm)
{
    int myrank = 0;
    int ccsize = 0;

    PMPI_Comm_rank(comm, &myrank);
    PMPI_Comm_size(comm, &ccsize);

    std::vector<uint64_t> res(ccsize, 0);

    int ret = PMPI_Allgather(&hash,      1, MPI_UINT64_T,
                             res.data(), 1, MPI_UINT64_T, comm);

    if (ret != MPI_SUCCESS)
        return -1;

    int hrank = 0;

    for (int i = 0; i < myrank; ++i)
        if (hash == res[i])
            ++hrank;

    return hrank;
}

int get_rank_for_node()
{
    constexpr std::size_t max_len = 1024;
    char hostname[max_len];
    if (gethostname(hostname, max_len) < 0)
        return -1;

    int initialized = 0;
    PMPI_Initialized(&initialized);
    if (!initialized)
        return 0;

    return ::get_rank_for_hash(std::hash<std::string>{}(std::string(hostname)), MPI_COMM_WORLD);
}

} // namespace [anonymous]

namespace cali
{

namespace machine
{

int get_rank_for(MachineLevel level)
{
    switch (level) {
    case MachineLevel::Process:
        return 0;
    case MachineLevel::Node:
        return ::get_rank_for_node();
    default:
        Log(0).stream() << "machine::get_rank_for(MachineLevel): level "
                        << level << " is not supported"
                        << std::endl;
    }

    return -1;
}

} // namespace machine

}
