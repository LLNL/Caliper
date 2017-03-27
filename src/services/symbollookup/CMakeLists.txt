include_directories(${DYNINST_INCLUDE_DIR})

set(CALIPER_SYMBOLLOOKUP_SOURCES
  SymbolLookup.cpp)

add_library(caliper-symbollookup OBJECT ${CALIPER_SYMBOLLOOKUP_SOURCES})

add_service_objlib("caliper-symbollookup")
add_caliper_service("symbollookup CALIPER_HAVE_DYNINST")
