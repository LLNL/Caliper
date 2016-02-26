include_directories(${LIBCURL_INCLUDE_DIR})

set(CALIPER_NETOUT_SOURCES
    NetOut.cpp)

add_library(caliper-netout OBJECT ${CALIPER_NETOUT_SOURCES})

add_service_objlib("caliper-netout")
