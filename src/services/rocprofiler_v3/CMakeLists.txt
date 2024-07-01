set(CALIPER_ROCPROFILER_SOURCES
    RocProfiler.cpp)

add_library(caliper-rocprofiler OBJECT ${CALIPER_ROCPROFILER_SOURCES})
target_link_libraries(caliper-rocprofiler PRIVATE rocprofiler-sdk::rocprofiler-sdk)

add_service_objlib("caliper-rocprofiler")
add_caliper_service("rocprofiler CALIPER_HAVE_ROCM")
