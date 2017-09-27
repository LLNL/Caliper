include_directories(${GOTCHA_INCLUDE_DIR})

set(CALIPER_ALLOC_SOURCES
    AllocTracker.cpp
    AllocService.cpp)

add_library(caliper-alloc OBJECT ${CALIPER_ALLOC_SOURCES})

add_service_objlib("caliper-alloc")
add_caliper_service("alloc CALIPER_HAVE_GOTCHA")
