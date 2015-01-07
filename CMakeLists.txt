#
# Caliper
# 

project (caliper)
cmake_minimum_required(VERSION 2.8)

find_package(Threads)

# Set OMPT directory manually for now. Use automatic check later.
set(OMPT_DIR "/g/g90/boehme3/local/ompt-intel-openmp/default")
set(OMPT_FOUND true)

if(OMPT_FOUND)
  set(CALIPER_HAVE_OMPT true)
endif()

# Create a config header file
configure_file(
  ${PROJECT_SOURCE_DIR}/caliper-config.h.in
  ${PROJECT_BINARY_DIR}/caliper-config.h)
install(FILES ${PROJECT_BINARY_DIR}/caliper-config.h DESTINATION include)

add_subdirectory(src)
add_subdirectory(test)

