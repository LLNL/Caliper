set(CALIPER_MPI_CONTROLLER_SOURCES
  controllers/LoopReportController.cpp
  controllers/SpotController.cpp
  controllers/SpotV1Controller.cpp)

list(APPEND CALIPER_MPI_SOURCES ${CALIPER_MPI_CONTROLLER_SOURCES})
set(CALIPER_MPI_SOURCES ${CALIPER_MPI_SOURCES} PARENT_SCOPE)
