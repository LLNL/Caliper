include_directories("..")

set(C_UTIL_SOURCES
    vlenc.c)
set(C_UTIL_HEADERS
    vlenc.h)

add_library(c-util OBJECT ${C_UTIL_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET c-util PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()

install(FILES ${C_UTIL_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/caliper/c-util)
