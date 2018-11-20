#
# - Find libunwind
#
# LIBUNWIND_PREFIX      - Set to the libunwind installation directory
#
# LIBUNWIND_INCLUDE_DIR - Path to libunwind.h
# LIBUNWIND_LIBRARIES   - List of libraries for using libunwind
# LIBUNWIND_FOUND       - True if libunwind was found

find_path(LIBUNWIND_PREFIX
  include/libunwind.h)

find_library(LIBUNWIND_LIBRARIES
  NAMES unwind
  HINTS ${LIBUNWIND_PREFIX}/lib)

find_path(LIBUNWIND_INCLUDE_DIRS
  NAMES libunwind.h
  HINTS ${LIBUNWIND_PREFIX}/include)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libunwind DEFAULT_MSG LIBUNWIND_LIBRARIES LIBUNWIND_INCLUDE_DIRS)

mark_as_advanced(LIBUNWIND_LIBRARIES LIBUNWIND_INCLUDE_DIRS)
