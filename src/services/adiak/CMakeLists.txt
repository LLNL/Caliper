include_directories(${adiak_INCLUDE_DIRS})

set(CALIPER_ADIAK_SOURCES
  AdiakImport.cpp)

add_library(caliper-adiak OBJECT ${CALIPER_ADIAK_SOURCES})

add_service_objlib("caliper-adiak")
add_caliper_service("adiak_import CALIPER_HAVE_ADIAK")
