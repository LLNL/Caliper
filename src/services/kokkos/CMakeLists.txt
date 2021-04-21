set(CALIPER_KOKKOS_SOURCES
  KokkosProfilingSymbols.cpp
  KokkosLookup.cpp
  KokkosTime.cpp
     )
if(WITH_ADIAK)
include_directories(${adiak_INCLUDE_DIRS})
    list(APPEND CALIPER_KOKKOS_SOURCES "KokkosMetadataService.cpp")
    add_caliper_service("kokkosadiak")
endif()
add_library(caliper-kokkos OBJECT ${CALIPER_KOKKOS_SOURCES})

add_service_objlib("caliper-kokkos")
add_caliper_service("kokkostime")
add_caliper_service("kokkoslookup")
