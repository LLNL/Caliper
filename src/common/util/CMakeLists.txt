set(UTIL_SOURCES
  util/format_util.cpp
  util/parse_util.cpp
  util/unitfmt.c
  util/vlenc.c)

list(APPEND CALIPER_COMMON_SOURCES ${UTIL_SOURCES})
set(CALIPER_COMMON_SOURCES ${CALIPER_COMMON_SOURCES} PARENT_SCOPE)
