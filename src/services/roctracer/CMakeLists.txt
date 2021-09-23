set(CALIPER_ROCTRACER_SOURCES
  RocTracer.cpp)

add_library(caliper-roctracer OBJECT ${CALIPER_ROCTRACER_SOURCES})
target_compile_definitions(caliper-roctracer PRIVATE __HIP_PLATFORM_AMD__)
target_include_directories(caliper-roctracer PRIVATE ${ROCTRACER_INCLUDE_DIRS})

add_service_objlib("caliper-roctracer")
add_caliper_service("roctracer CALIPER_HAVE_ROCTRACER")
