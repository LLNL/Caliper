// Test MpiChannelManager

#include <caliper/cali.h>
#include <caliper/cali-mpi.h>
#include <caliper/Caliper.h>

#include <caliper/ConfigManager.h>
#include <caliper/MpiChannelManager.h>

#include <mpi.h>

#include <iostream>
#include <tuple>

int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    int worldrank = 0;
    MPI_Comm subcomm;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldrank);
    MPI_Comm_split(MPI_COMM_WORLD, worldrank % 2, worldrank, &subcomm);

    cali::ConfigManager mgr;
    if (argc > 1)
        mgr.add(argv[1]);

    if (mgr.error()) {
        std::cerr << "Caliper error: " << mgr.error_msg() << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    cali::MpiChannelManager mpimgr(subcomm);
    mpimgr.add(mgr);
    mpimgr.start();

    {
        CALI_CXX_MARK_FUNCTION;

        MPI_Barrier(subcomm);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    mpimgr.stop();

    if (worldrank % 2 == 0)
        mpimgr.collective_flush();

    MPI_Comm_free(&subcomm);
    MPI_Finalize();
}