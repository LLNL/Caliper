set(CMAKE_C_COMPILER   "/usr/tce/packages/xl/xl-2020.06.25/bin/xlc" CACHE PATH "")
set(CMAKE_CXX_COMPILER "/usr/tce/packages/xl/xl-2020.06.25/bin/xlC" CACHE PATH "")
set(CMAKE_Fortran_COMPILER "/usr/tce/packages/xl/xl-2020.06.25/bin/xlf" CACHE PATH "")

# set(MPI_C_COMPILER     "/usr/tce/packages/spectrum-mpi/spectrum-mpi-rolling-release-xl-2019.02.07/bin/mpicc" CACHE PATH "")
# set(MPI_CXX_COMPILER   "/usr/tce/packages/spectrum-mpi/spectrum-mpi-rolling-release-xl-2019.02.07/bin/mpicxx" CACHE PATH "")

set(CUDA_TOOLKIT_ROOT_DIR "/usr/tce/packages/cuda/cuda-10.1.243" CACHE PATH "")
set(CUPTI_PREFIX "/usr/tce/packages/cuda/cuda-10.1.243/extras/CUPTI" CACHE PATH "")

# Our f2003 requires some extra options for xlf. Skip it for now. 
set(WITH_FORTRAN  Off CACHE BOOL "")
set(WITH_LIBUNWIND On  CACHE BOOL "")
set(WITH_NVPROF   On  CACHE BOOL "")
set(WITH_CUPTI    On  CACHE BOOL "")
set(WITH_PAPI     Off CACHE BOOL "")
set(WITH_LIBDW    On  CACHE BOOL "")
set(WITH_LIBPFM   Off CACHE BOOL "")
set(WITH_SAMPLER  On  CACHE BOOL "")
set(WITH_MPI      On  CACHE BOOL "")
set(WITH_GOTCHA   On  CACHE BOOL "")
set(WITH_VTUNE    Off CACHE BOOL "")

set(WITH_DOCS     Off CACHE BOOL "")
set(BUILD_TESTING On  CACHE BOOL "")

set(RUN_MPI_TESTS Off CACHE BOOL "")
