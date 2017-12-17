set(C_UTIL_SOURCES
    unitfmt.c
    vlenc.c)

add_library(c-util OBJECT ${C_UTIL_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET c-util PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
