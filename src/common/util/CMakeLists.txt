set(UTIL_SOURCES
    parse_util.cpp)

add_library(util OBJECT ${UTIL_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET util PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
