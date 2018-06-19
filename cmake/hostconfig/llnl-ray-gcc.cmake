set(CMAKE_C_COMPILER   "/usr/tcetmp/bin/gcc" CACHE PATH "C Compiler (gcc 4.9.3)")
set(CMAKE_CXX_COMPILER "/usr/tcetmp/bin/g++" CACHE PATH "C++ Compiler (g++ 4.9.3)")

set(MPI_C_COMPILER   "/usr/tcetmp/bin/mpigcc" CACHE PATH "")
set(MPI_CXX_COMPILER "/usr/tcetmp/bin/mpig++" CACHE PATH "")

set(CUDA_TOOLKIT_ROOT_DIR "/usr/local/cuda" CACHE PATH "")

set(CUPTI_PREFIX "/usr/local/cuda/extras/CUPTI" CACHE PATH "")

set(WITH_CALLPATH Off CACHE BOOL "")
set(WITH_NVPROF   On  CACHE BOOL "")
set(WITH_CUPTI    On  CACHE BOOL "")
set(WITH_PAPI     Off CACHE BOOL "")
set(WITH_DYNINST  Off CACHE BOOL "")
set(WITH_SAMPLER  On  CACHE BOOL "")
set(WITH_LIBPFM   Off CACHE BOOL "")
set(WITH_MPI      On  CACHE BOOL "")
set(WITH_MPIT     Off CACHE BOOL "")
set(WITH_GOTCHA   Off CACHE BOOL "")

set(WITH_DOCS     Off CACHE BOOL "")
