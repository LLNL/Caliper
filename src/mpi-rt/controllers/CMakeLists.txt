set(CALIPER_MPI_CONTROLLER_SOURCES
  SpotController.cpp
  SpotV1Controller.cpp)

add_library(caliper-mpi-controllers OBJECT ${CALIPER_MPI_CONTROLLER_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-mpi-controllers PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

if (${CALIPER_HAVE_ADIAK})
  target_include_directories(caliper-mpi-controllers PRIVATE ${adiak_INCLUDE_DIRS})
endif()
