set(CALIPER_KOKKOSLOOKUP_SOURCES
  KokkosLookup.cpp
  KokkosProfilingSymbols.cpp)

add_library(caliper-kokkoslookup OBJECT ${CALIPER_KOKKOSLOOKUP_SOURCES})

# prevents undefined boost::system kokkoss in Caliper
target_compile_definitions(caliper-kokkoslookup
  PRIVATE
  BOOST_SYSTEM_NO_DEPRECATED
  BOOST_ERROR_CODE_HEADER_ONLY)

add_service_objlib("caliper-kokkoslookup")
add_caliper_service("kokkoslookup")
