set(CALIPER_MPI_CONTROLLER_SOURCES
  controllers/LoopReportControllerMpi.cpp
  controllers/SpotControllerMpi.cpp
  controllers/SpotV1Controller.cpp)

list(APPEND CALIPER_MPI_SOURCES ${CALIPER_MPI_CONTROLLER_SOURCES})
set(CALIPER_MPI_SOURCES ${CALIPER_MPI_SOURCES} PARENT_SCOPE)
