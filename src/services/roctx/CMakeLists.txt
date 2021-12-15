set(CALIPER_ROCTX_SOURCES
  RocTX.cpp)

add_library(caliper-roctx OBJECT ${CALIPER_ROCTX_SOURCES})
target_include_directories(caliper-roctx PRIVATE ${ROCTX_INCLUDE_DIRS})

add_service_objlib("caliper-roctx")
add_caliper_service("roctx CALIPER_HAVE_ROCTX")
