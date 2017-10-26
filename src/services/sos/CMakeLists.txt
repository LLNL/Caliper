include_directories(${SOSFlow_INCLUDE_DIR})

set(CALIPER_SOS_SOURCES
  Sos.cpp)

add_library(caliper-sos OBJECT ${CALIPER_SOS_SOURCES})

add_service_objlib("caliper-sos")
add_caliper_service("sos CALIPER_HAVE_SOS")
