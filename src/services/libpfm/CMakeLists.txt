include_directories(${LIBPFM_INCLUDE_DIR})

set(CALIPER_LIBPFM_SOURCES
    Libpfm.cpp
    perf_postprocessing.cpp
    perf_util.c)

add_library(caliper-libpfm OBJECT ${CALIPER_LIBPFM_SOURCES})

add_service_objlib("caliper-libpfm")
add_caliper_service("libpfm CALIPER_HAVE_LIBPFM")
