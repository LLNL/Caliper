include_directories(${PAPI_INCLUDE_DIRS})

set(CALIPER_PAPI_SOURCES
    Papi.cpp)

add_library(caliper-papi OBJECT ${CALIPER_PAPI_SOURCES})

add_service_objlib("caliper-papi")
add_caliper_service("papi CALIPER_HAVE_PAPI")
