set(UTIL_SOURCES
  format_util.cpp
  parse_util.cpp
  unitfmt.c
  vlenc.c)

add_library(caliper-util OBJECT ${UTIL_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-util PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
