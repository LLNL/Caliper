#
# Caliper
# 

project (caliper)
cmake_minimum_required(VERSION 2.8)

find_package(Threads)

# Set OMPT directory manually for now. Use automatic check later.
set(OMPT_DIR "/g/g90/boehme3/local/ompt-intel-openmp/default")
set(OMPT_FOUND true)

add_subdirectory(src)
add_subdirectory(test)

