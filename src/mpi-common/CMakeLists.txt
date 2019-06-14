include_directories(${MPI_CXX_INCLUDE_PATH})

set(CALIPER_MPI_SOURCES
  aggregate_over_mpi.cpp)

add_library(caliper-mpi-common OBJECT ${CALIPER_MPI_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-mpi-common PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

# set_target_properties(caliper-mpi-common PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
# set_target_properties(caliper-mpi-common PROPERTIES VERSION ${CALIPER_VERSION})

# target_link_libraries(caliper-mpi-common PUBLIC caliper-reader)
# target_link_libraries(caliper-mpi-common PRIVATE ${MPI_C_LIBRARIES} ${MPI_CXX_LIBRARIES})

# install(TARGETS caliper-mpi-common
#   EXPORT caliper
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
