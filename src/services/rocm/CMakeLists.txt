set(CALIPER_ROCM_SOURCES
  RocTX.cpp)

add_library(caliper-rocm OBJECT ${CALIPER_ROCM_SOURCES})

target_include_directories(caliper-rocm PRIVATE ${ROCM_INCLUDE_DIRS})

add_service_objlib("caliper-rocm")

add_caliper_service("roctx CALIPER_HAVE_ROCM")
