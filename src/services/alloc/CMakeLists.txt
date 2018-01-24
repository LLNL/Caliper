set(CALIPER_ALLOC_SOURCES
    AllocService.cpp)

add_library(caliper-alloc OBJECT ${CALIPER_ALLOC_SOURCES})

add_service_objlib("caliper-alloc")
add_caliper_service("alloc")
