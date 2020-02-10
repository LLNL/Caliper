set(CALIPER_KOKKOS_SOURCES
  KokkosProfilingSymbols.cpp
  KokkosLookup.cpp
  KokkosTime.cpp
)

add_library(caliper-kokkos OBJECT ${CALIPER_KOKKOS_SOURCES})

add_service_objlib("caliper-kokkos")
add_caliper_service("kokkostime")
add_caliper_service("kokkoslookup")
