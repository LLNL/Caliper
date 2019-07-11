include_directories(${adiak_INCLUDE_DIRS})

set(CALIPER_ADIAK_SOURCES
  AdiakExport.cpp
  AdiakImport.cpp)

add_library(caliper-adiak OBJECT ${CALIPER_ADIAK_SOURCES})

add_service_objlib("caliper-adiak")

add_caliper_service("adiak_export CALIPER_HAVE_ADIAK")
add_caliper_service("adiak_import CALIPER_HAVE_ADIAK")

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
