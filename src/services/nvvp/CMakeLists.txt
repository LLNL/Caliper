include_directories(${CUDA_INCLUDE_DIRS})

set(CALIPER_NVVP_SOURCES
    nvvp.cpp)

add_library(caliper-nvvp OBJECT ${CALIPER_NVVP_SOURCES})

add_service_objlib(caliper-nvvp)
add_caliper_service("nvvp CALIPER_HAVE_NVVP")
