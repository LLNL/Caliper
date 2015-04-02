#
# Caliper
# 

project (caliper)
cmake_minimum_required(VERSION 2.8)

# Add our module directory to the include path.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(FindLibunwind)

if (LIBUNWIND_FOUND)
  set(CALIPER_HAVE_LIBUNWIND TRUE)
endif()

find_package(Threads)

# Find OMPT header
find_path(OMPT_INCLUDE_DIR ompt.h
  PATH_SUFFIXES include
  HINTS $ENV{OMPT_DIR} ${OMPT_DIR})

if (OMPT_INCLUDE_DIR)
  set(OMPT_FOUND TRUE)  
  set(CALIPER_HAVE_OMPT TRUE)
  message(STATUS "OpenMP tools interface header ompt.h found in " ${OMPT_INCLUDE_DIR})
else()
  message(STATUS "OpenMP tools interface header ompt.h not found")
endif()

# Create a config header file
configure_file(
  ${PROJECT_SOURCE_DIR}/caliper-config.h.in
  ${PROJECT_BINARY_DIR}/caliper-config.h)
install(FILES ${PROJECT_BINARY_DIR}/caliper-config.h DESTINATION include)

add_subdirectory(src)
add_subdirectory(test)

