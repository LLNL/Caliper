set(CALIPER_SYMBOLLOOKUP_SOURCES
  SymbolLookup.cpp)

if (CALIPER_HAVE_DYNINST)
  list(APPEND CALIPER_SYMBOLLOOKUP_SOURCES
    LookupDyninst.cpp)
else()
  list(APPEND CALIPER_SYMBOLLOOKUP_SOURCES
    LookupDlAddr.cpp)
endif()

add_library(caliper-symbollookup OBJECT ${CALIPER_SYMBOLLOOKUP_SOURCES})

if (CALIPER_HAVE_DYNINST)
  target_include_directories(caliper-symbollookup
    PRIVATE ${DYNINST_INCLUDE_DIR})
  # prevents undefined boost::system symbols in Caliper
  target_compile_definitions(caliper-symbollookup
    PRIVATE
    BOOST_SYSTEM_NO_DEPRECATED
    BOOST_ERROR_CODE_HEADER_ONLY)
endif()

add_service_objlib("caliper-symbollookup")
add_caliper_service("symbollookup CALIPER_HAVE_SYMBOLLOOKUP")
