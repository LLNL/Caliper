#
# - Find libpfm
#
# LIBPFM_INCLUDE_DIR - Path to libpfm.h
# LIBPFM_LIBRARY     - List of libraries for using libpfm
# LIBPFM_FOUND       - True if libpfm was found

set(LIBPFM_INSTALL "" CACHE PATH "libpfm install directory")

if(LIBPFM_INSTALL)
    # install dir specified, only search them
    find_path(LIBPFM_INCLUDE_DIR "perfmon/pfmlib_perf_event.h"
            PATHS ${LIBPFM_INSTALL} ${LIBPFM_INSTALL}/include
            NO_DEFAULT_PATH)

    find_library(LIBPFM_LIBRARY pfm
            PATHS ${LIBPFM_INSTALL} ${LIBPFM_INSTALL}/lib
            NO_DEFAULT_PATH)
else()
    # no install dir specified, look in default and PAPI
    find_path(LIBPFM_INCLUDE_DIR "perfmon/pfmlib_perf_event.h"
            HINTS ${PAPI_INCLUDE_DIRS} ${PAPI_INCLUDE_DIRS}/libpfm4/include)

    find_library(LIBPFM_LIBRARY pfm
            HINTS ${PAPI_LIBRARIES} ${PAPI_LIBRARIES}/libpfm4/include)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libpfm DEFAULT_MSG LIBPFM_LIBRARY LIBPFM_INCLUDE_DIR)

mark_as_advanced(LIBPFM_LIBRARY LIBPFM_INCLUDE_DIR)
