set(WRAP ${CMAKE_CURRENT_SOURCE_DIR}/wrap.py)
include(${PROJECT_SOURCE_DIR}/cmake/WrapConfig.cmake)

add_wrapped_file(Wrapper.cpp Wrapper.w)

set(CALIPER_PMPI_SOURCES
    Wrapper.cpp)

add_library(caliper-pmpi OBJECT ${CALIPER_PMPI_SOURCES})

add_mpi_service_objlib(caliper-pmpi)
