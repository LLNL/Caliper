set(CALIPER_UMPIRE_SOURCES
  UmpireStatistics.cpp)

add_library(caliper-umpire OBJECT ${CALIPER_UMPIRE_SOURCES})
target_link_libraries(caliper-umpire PRIVATE umpire)
  
add_service_objlib("caliper-umpire")
add_caliper_service("umpire CALIPER_HAVE_UMPIRE")
