// Copyright (c) 2015-2021, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

//   This example demonstrates how to use make_collective_output_channel()
// to gather and write profiling results into a custom C++ stream in MPI.

#include <caliper/cali.h>
#include <caliper/CollectiveOutputChannel.h>

#include <mpi.h>

#include <fstream>
#include <iostream>
#include <tuple>


int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    std::shared_ptr<cali::CollectiveOutputChannel> channel;
    std::string errmsg;

    //
    // Create a "runtime-report" channel and return a control object.

    std::tie(channel, errmsg) =
        cali::make_collective_output_channel("runtime-report(profile.mpi)");

    //
    //   Check if the channel was created successfully. If not, the second
    // return value contains an error message.

    if (!channel) {
        std::cerr << "Error: " << errmsg << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    channel->start();

    CALI_MARK_BEGIN("work");
    // ...
    CALI_MARK_END("work");

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    std::ofstream os;
    if (rank == 0) {
        os.open("report.txt");
    }

    //
    //   Gather and flush channel output into the given std::ostream on
    // MPI_COMM_WORLD. This is a collective operation on MPI_COMM_WORLD.

    channel->collective_flush(os, MPI_COMM_WORLD);

    MPI_Finalize();
}