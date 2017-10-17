set(CALIPER_MPIT_SOURCES
  MpitService.cpp)

add_service_sources(${CALIPER_MPIT_SOURCES})

add_caliper_service("mpit CALIPER_HAVE_MPIT")
