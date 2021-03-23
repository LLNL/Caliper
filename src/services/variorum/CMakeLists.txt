include_directories(${VARIORUM_INCLUDE_DIRS})

set(CALIPER_VARIORUM_SOURCES
    VariorumService.cpp)

add_library(caliper-variorum OBJECT ${CALIPER_VARIORUM_SOURCES})

add_service_objlib("caliper-variorum")
add_caliper_service("variorum CALIPER_HAVE_VARIORUM")
