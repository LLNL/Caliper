// Test Caliper MPI runtime: mpi init before caliper init

#include <caliper/cali.h>
#include <caliper/cali-mpi.h>
#include <caliper/cali-manager.h>

#include <mpi.h>

int main(int argc, char* argv[])
{
    cali_mpi_init();

    MPI_Init(&argc, &argv);

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    cali::ConfigManager mgr;

    if (argc > 1)
        mgr.add(argv[1]);
    if (mgr.error()) {
        if (rank == 0)
            std::cerr << mgr.error_msg() << std::endl;

        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    mgr.start();

    {
        CALI_CXX_MARK_FUNCTION;

        // some MPI functions to test the wrapper blacklist/whitelist

        MPI_Barrier(MPI_COMM_WORLD);

        int val = 42;

        MPI_Bcast(&val, 1, MPI_INT, 0, MPI_COMM_WORLD);

        int in  = val, out;

        MPI_Reduce(&in, &out, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);
    }

    mgr.flush();

    MPI_Finalize();
}
