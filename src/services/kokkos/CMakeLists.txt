set(CALIPER_KOKKOSCOMMON_SOURCES
  KokkosProfilingSymbols.cpp)


set(CALIPER_KOKKOSLOOKUP_SOURCES
  KokkosLookup.cpp
  )

#set(CALIPER_KOKKOSTIME_SOURCES
#  KokkosTimers.cpp
#)

add_library(caliper-kokkos-common OBJECT ${CALIPER_KOKKOSCOMMON_SOURCES})
add_library(caliper-kokkos-lookup OBJECT ${CALIPER_KOKKOSLOOKUP_SOURCES})

# prevents undefined boost::system kokkoss in Caliper
target_compile_definitions(caliper-kokkos-lookup
  PRIVATE
  BOOST_SYSTEM_NO_DEPRECATED
  BOOST_ERROR_CODE_HEADER_ONLY)

add_service_objlib("caliper-kokkos-common")
add_service_objlib("caliper-kokkos-lookup")
add_caliper_service("kokkoslookup")
