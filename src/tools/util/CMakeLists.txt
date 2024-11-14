set(CALIPER_TOOLS_UTIL_SOURCES
  Args.cpp)

add_library(caliper-tools-util OBJECT ${CALIPER_TOOLS_UTIL_SOURCES})
target_compile_features(caliper-tools-util PUBLIC cxx_std_11)
