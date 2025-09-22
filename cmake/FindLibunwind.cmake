#
# - Find libunwind
#
# LIBUNWIND_PREFIX      - Set to the libunwind installation directory
#
# LIBUNWIND_INCLUDE_DIRS - Path to libunwind.h
# LIBUNWIND_LIBRARIES   - List of libraries for using libunwind
# LIBUNWIND_FOUND       - True if libunwind was found

if(LIBUNWIND_PREFIX)
  # When prefix is explicitly provided, only look there
  find_library(LIBUNWIND_LIBRARIES
    NAMES unwind
    PATHS ${LIBUNWIND_PREFIX}/lib
    NO_DEFAULT_PATH)

  find_path(LIBUNWIND_INCLUDE_DIRS
    NAMES libunwind.h
    PATHS ${LIBUNWIND_PREFIX}/include
    NO_DEFAULT_PATH)

  if(NOT LIBUNWIND_LIBRARIES OR NOT LIBUNWIND_INCLUDE_DIRS)
    message(WARNING "LIBUNWIND_PREFIX was set to '${LIBUNWIND_PREFIX}' but libunwind was not found there")
  endif()
else()
  # Try to find libunwind in standard locations
  find_path(LIBUNWIND_PREFIX
    include/libunwind.h)

  find_library(LIBUNWIND_LIBRARIES
    NAMES unwind
    HINTS ${LIBUNWIND_PREFIX}/lib)

  find_path(LIBUNWIND_INCLUDE_DIRS
    NAMES libunwind.h
    HINTS ${LIBUNWIND_PREFIX}/include)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libunwind DEFAULT_MSG LIBUNWIND_LIBRARIES LIBUNWIND_INCLUDE_DIRS)

mark_as_advanced(LIBUNWIND_LIBRARIES LIBUNWIND_INCLUDE_DIRS)
