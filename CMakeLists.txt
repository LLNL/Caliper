#
# Caliper
# 

project (caliper)
cmake_minimum_required(VERSION 2.8)

find_package(Threads)

add_subdirectory(src)
add_subdirectory(test)

