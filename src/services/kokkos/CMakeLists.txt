set(CALIPER_KOKKOS_SOURCES
  KokkosProfilingSymbols.cpp
  KokkosLookup.cpp
  KokkosTime.cpp
)

add_library(caliper-kokkos OBJECT ${CALIPER_KOKKOS_SOURCES})
target_include_directories(caliper-kokkos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_service_objlib("caliper-kokkos")
add_caliper_service("kokkostime")
add_caliper_service("kokkoslookup")
