set(CALIPER_INTERFACE_SOURCES
  BufferedRegionProfile.cpp
  wrapAnnotation.cpp
  wrapBufferedRegionProfile.cpp
  wrapConfigManager.cpp
  wrapScopeAnnotation.cpp
  utilCaliper.cpp)

if (CALIPER_HAVE_FORTRAN)
  list(APPEND CALIPER_INTERFACE_SOURCES
    wrapfcaliper.F)
  set_source_files_properties(wrapfcaliper.F PROPERTIES Fortran_FORMAT FREE)
endif()

add_library(caliper-interface OBJECT ${CALIPER_INTERFACE_SOURCES})
target_compile_features(caliper-interface PUBLIC cxx_std_11)

if (CALIPER_HAVE_FORTRAN)
  target_include_directories(
    caliper-interface
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_Fortran_MODULE_DIRECTORY}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/caliper/fortran>"
  )
endif()