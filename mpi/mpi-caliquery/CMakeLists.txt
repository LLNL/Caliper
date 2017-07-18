add_executable(mpi-caliquery mpi-caliquery.cpp)

target_link_libraries(mpi-caliquery caliper-mpi caliper-tools-util caliper-mpiwrap caliper)

install(TARGETS mpi-caliquery DESTINATION ${CMAKE_INSTALL_BINDIR})
