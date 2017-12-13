include_directories("../../src/tools/cali-query")

add_executable(mpi-caliquery
    $<TARGET_OBJECTS:query-common>
    mpi-caliquery.cpp)

target_link_libraries(mpi-caliquery caliper-mpi caliper-mpi-common caliper-tools-util caliper)

install(TARGETS mpi-caliquery DESTINATION ${CMAKE_INSTALL_BINDIR})
