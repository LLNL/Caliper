include_directories("include")
include_directories(${MPI_CXX_INCLUDE_PATH})

add_subdirectory(mpi-common)
add_subdirectory(mpi-rt)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
