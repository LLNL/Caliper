set(CALIPER_MPI_SOURCES
    MpiWrap.cpp)

add_service_sources(${CALIPER_MPI_SOURCES})

add_caliper_service("mpi CALIPER_HAVE_MPI")
