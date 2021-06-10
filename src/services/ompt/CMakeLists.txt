set(CALIPER_OMPT_SOURCES
  OmptService.cpp)

add_service_sources(${CALIPER_OMPT_SOURCES})
add_caliper_service("ompt")
