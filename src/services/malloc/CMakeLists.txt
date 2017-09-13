include_directories(${GOTCHA_INCLUDE_DIR})

set(CALIPER_MALLOC_SOURCES
    MallocService.cpp)

add_library(caliper-malloc OBJECT ${CALIPER_MALLOC_SOURCES})

add_service_objlib("caliper-malloc")
add_caliper_service("malloc CALIPER_HAVE_GOTCHA")
