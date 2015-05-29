
#include <Caliper.h>

#include <mpi.h>

namespace cali
{
    extern Attribute mpifn_attr;
    extern Attribute mpirank_attr;

    extern bool      mpi_enabled;
}

using namespace cali;

{{fn func MPI_Init}}{
    // Make sure Caliper is initialized
    Caliper* c = Caliper::instance();    

    {{callfn}}

    if (mpi_enabled) {
        int rank = 0;
        PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
        c->set(mpirank_attr, Variant(rank));
    }
}{{endfn}}

{{fn func MPI_Init_thread}}{
    // Make sure Caliper is initialized
    Caliper* c = Caliper::instance();    

    {{callfn}}

    if (mpi_enabled) {
        int rank = 0;
        PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
        c->set(mpirank_attr, Variant(rank));
    }
}{{endfn}}

// Wrap all MPI functions

{{fnall func MPI_Init MPI_Init_thread}}{
    if (mpi_enabled) {
        Caliper* c = Caliper::instance();
        c->begin(mpifn_attr, Variant(CALI_TYPE_STRING, "{{func}}", strlen("{{func}}")));
        {{callfn}}
        c->end(mpifn_attr);
    } else {
        {{callfn}}
    }
}{{endfnall}}
