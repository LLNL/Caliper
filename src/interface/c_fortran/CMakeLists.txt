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
    wrapfcaliper.F08)
   set_source_files_properties(wrapfcaliper.F08 PROPERTIES Fortran_FORMAT FREE)  
   set_source_files_properties(wrapfcaliper.F08 PROPERTIES LANGUAGE Fortran)  
endif()

add_library(caliper-interface OBJECT ${CALIPER_INTERFACE_SOURCES})

if (${BUILD_SHARED_LIBS})
  set_property(TARGET caliper-interface PROPERTY POSITION_INDEPENDENT_CODE TRUE)
endif()
