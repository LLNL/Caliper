set(CALIPER_INTERFACE_HEADERS
  typesCaliper.h
  wrapAnnotation.h
  wrapConfigManager.h
  wrapScopeAnnotation.h)

set(CALIPER_INTERFACE_SOURCES
  wrapAnnotation.cpp
  wrapCaliper.cpp
  wrapConfigManager.cpp
  wrapScopeAnnotation.cpp)

if (WITH_FORTRAN)
  list(APPEND CALIPER_INTERFACE_SOURCES
    wrapfcaliper.f)
  set_source_files_properties(wrapfcaliper.f PROPERTIES Fortran_FORMAT FREE)  
endif()

add_library(caliper-interface OBJECT ${CALIPER_INTERFACE_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-interface PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
