include_directories(${DYNINST_INCLUDE_DIR})

set(CALIPER_INSTLOOKUP_SOURCES
  InstLookup.cpp)

add_library(caliper-instlookup OBJECT ${CALIPER_INSTLOOKUP_SOURCES})

# prevents undefined boost::system symbols in Caliper
target_compile_definitions(caliper-instlookup
  PRIVATE
  BOOST_SYSTEM_NO_DEPRECATED
  BOOST_ERROR_CODE_HEADER_ONLY)

add_service_objlib("caliper-instlookup")
add_caliper_service("instlookup CALIPER_HAVE_DYNINST")
