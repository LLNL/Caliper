include_directories("../../common")
include_directories("../../caliper")

include_directories(${MPI_C_INCLUDE_PATH})

set(WRAP ${CMAKE_CURRENT_SOURCE_DIR}/wrap.py)
include(${PROJECT_SOURCE_DIR}/cmake/WrapConfig.cmake)

add_wrapped_file(Wrapper.cpp Wrapper.w)

set(CALIPER_MPI_SOURCES
    MpiWrap.cpp)
set(CALIPER_MPIWRAP_SOURCES
    Wrapper.cpp)

add_service_sources(${CALIPER_MPI_SOURCES})

add_library(caliper-mpiwrap ${CALIPER_MPIWRAP_SOURCES})

# add_library(caliper-mpi ${CALIPER_MPI_SOURCES})
# target_link_libraries(caliper-mpi caliper-common)
# target_link_libraries(caliper-mpi caliper)

target_link_libraries(caliper-mpiwrap caliper-common)
target_link_libraries(caliper-mpiwrap caliper)
target_link_libraries(caliper-mpiwrap ${MPI_C_LIBRARIES})

install(TARGETS caliper-mpiwrap
  EXPORT caliper
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)
