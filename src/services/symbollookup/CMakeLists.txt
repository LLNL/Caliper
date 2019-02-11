include_directories(${DYNINST_INCLUDE_DIR})

set(CALIPER_SYMBOLLOOKUP_SOURCES
  SymbolLookup.cpp)

add_library(caliper-symbollookup OBJECT ${CALIPER_SYMBOLLOOKUP_SOURCES})

# prevents undefined boost::system symbols in Caliper
target_compile_definitions(caliper-symbollookup
  PRIVATE
  BOOST_SYSTEM_NO_DEPRECATED
  BOOST_ERROR_CODE_HEADER_ONLY)

add_service_objlib("caliper-symbollookup")
add_caliper_service("symbollookup CALIPER_HAVE_DYNINST")
