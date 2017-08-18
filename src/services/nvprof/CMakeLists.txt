include_directories(${CUDA_INCLUDE_DIRS})

set(CALIPER_NVPROF_SOURCES
    NVProf.cpp)

add_library(caliper-nvprof OBJECT ${CALIPER_NVPROF_SOURCES})

add_service_objlib("caliper-nvprof")
add_caliper_service("nvprof CALIPER_HAVE_NVPROF")
