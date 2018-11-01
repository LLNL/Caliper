include_directories(${gotcha_INCLUDE_DIRS})

set(CALIPER_PTHREAD_SOURCES
    PthreadService.cpp)

add_library(caliper-pthread OBJECT ${CALIPER_PTHREAD_SOURCES})

add_service_objlib("caliper-pthread")
add_caliper_service("pthread CALIPER_HAVE_GOTCHA")
