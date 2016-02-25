#
# - Find libunwind
#
# LIBCURL_INCLUDE_DIR - Path to libunwind.h
# LIBCURL_LIBRARY     - List of libraries for using libunwind
# LIBCURL_FOUND       - True if libunwind was found

if(LIBCURL_INCLUDE_DIR)
  set(LIBCURL_FIND_QUIETLY true)
endif()

find_path(LIBCURL_INCLUDE_DIR curl.h)
find_library(LIBCURL_LIBRARY curl)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libcurl DEFAULT_MSG LIBCURL_LIBRARY LIBCURL_INCLUDE_DIR)

mark_as_advanced(LIBCURL_LIBRARY LIBCURL_INCLUDE_DIR)
