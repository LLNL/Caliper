// Test Caliper MPI runtime: Caliper init before mpi init

#include <caliper/cali.h>
#include <caliper/cali-mpi.h>

#include <mpi.h>

int main(int argc, char* argv[])
{
    cali_mpi_init();
    
    CALI_CXX_MARK_FUNCTION;
    
    MPI_Init(&argc, &argv);
    MPI_Barrier(MPI_COMM_WORLD);

    // some MPI functions to test the wrapper blacklist/whitelist

    int val = 42;
    
    MPI_Bcast(&val, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int in = val, out;

    MPI_Reduce(&in, &out, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
              
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();
}
