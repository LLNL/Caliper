include_directories("../cali-query")
include_directories(${MPI_CXX_INCLUDE_PATH})

add_executable(mpi-caliquery
  $<TARGET_OBJECTS:query-common>
  mpi-caliquery.cpp)

target_link_libraries(mpi-caliquery caliper caliper-tools-util)
target_link_libraries(mpi-caliquery ${MPI_CXX_LIBRARIES})

install(TARGETS mpi-caliquery DESTINATION ${CMAKE_INSTALL_BINDIR})
