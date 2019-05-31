set(CALIPER_MPI_CONTROLLER_SOURCES
  SpotController.cpp)

add_library(caliper-mpi-controllers OBJECT ${CALIPER_MPI_CONTROLLER_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-mpi-controllers PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
