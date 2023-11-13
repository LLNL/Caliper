include_directories(${LDMS_INCLUDE_DIRS})

set(CALIPER_LDMS_SOURCES
  LdmsForwarder.cpp)

add_library(caliper-ldms OBJECT ${CALIPER_LDMS_SOURCES})

add_service_objlib("caliper-ldms")
add_caliper_service("ldms CALIPER_HAVE_LDMS")


