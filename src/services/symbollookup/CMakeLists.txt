set(CALIPER_SYMBOLLOOKUP_SOURCES
  LookupLibdw.cpp
  SymbolLookup.cpp)

add_library(caliper-symbollookup OBJECT ${CALIPER_SYMBOLLOOKUP_SOURCES})
target_include_directories(caliper-symbollookup PRIVATE ${LIBDW_INCLUDE_DIR})

add_service_objlib("caliper-symbollookup")
add_caliper_service("symbollookup CALIPER_HAVE_LIBDW")
