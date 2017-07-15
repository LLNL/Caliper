set(CALIPER_MPI_SOURCES
  aggregate_over_mpi.cpp)

add_library(caliper-mpi ${CALIPER_MPI_SOURCES})

target_link_libraries(caliper-mpi caliper-reader)
target_link_libraries(caliper-mpi ${MPI_C_LIBRARIES} ${MPI_CXX_LIBRARIES})

install(TARGETS caliper-mpi
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
