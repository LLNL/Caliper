set(CALIPER_CRAYPAT_SOURCES
  CrayPAT.cpp)

add_library(caliper-craypat OBJECT ${CALIPER_CRAYPAT_SOURCES})
target_include_directories(caliper-craypat PRIVATE ${CRAYPAT_INCLUDE_DIRS})

add_service_objlib("caliper-craypat")
add_caliper_service("craypat CALIPER_HAVE_CRAYPAT")
