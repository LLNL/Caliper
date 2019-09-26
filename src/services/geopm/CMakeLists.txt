include_directories(SYSTEM ${GEOPM_INCLUDE_DIR})

set(CALIPER_GEOPM_SOURCES
    Geopm.cpp
    )

list(APPEND CALIPER_EXTERNAL_LIBS " -DDEBUGLEVEL=2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUGLEVEL=2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUGLEVEL=2")

add_library(caliper-libgeopm OBJECT ${CALIPER_GEOPM_SOURCES})

add_service_objlib("caliper-libgeopm")
add_caliper_service("geopm")
