include_directories(${CUDA_INCLUDE_DIRS})

set(CALIPER_NVTX_SOURCES
  Nvtx.cpp)

add_library(caliper-nvtx OBJECT ${CALIPER_NVTX_SOURCES})

add_service_objlib("caliper-nvtx")

add_caliper_service("nvtx CALIPER_HAVE_NVTX")
add_caliper_service("nvprof CALIPER_HAVE_NVTX")
