set(CALIPER_INTERFACE_SOURCES
  BufferedRegionProfile.cpp
  wrapAnnotation.cpp
  wrapBufferedRegionProfile.cpp
  wrapCaliper.cpp
  wrapConfigManager.cpp
  wrapScopeAnnotation.cpp
  utilCaliper.cpp)

if (WITH_FORTRAN)
  list(APPEND CALIPER_INTERFACE_SOURCES
    wrapfcaliper.F)
  set_source_files_properties(wrapfcaliper.F PROPERTIES Fortran_FORMAT FREE)  
endif()

add_library(caliper-interface OBJECT ${CALIPER_INTERFACE_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-interface PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
