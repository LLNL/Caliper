include_directories(${MPI_C_INCLUDE_PATH})

set(CALIPER_MPIT_SOURCES
  MpitService.cpp)
set(CALIPER_MPIT_MPI_SOURCES
  MpitServiceMPI.cpp)

add_service_sources(${CALIPER_MPIT_SOURCES})
add_mpi_service_sources(${CALIPER_MPIT_MPI_SOURCES})

add_caliper_service("mpit CALIPER_HAVE_MPIT")
