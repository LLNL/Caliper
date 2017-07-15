include_directories("include")
include_directories(${MPI_C_INCLUDE_PATH})

add_subdirectory(caliper-mpi)

if (WITH_TOOLS)
  add_subdirectory(mpi-caliquery)
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h")
