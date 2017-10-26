set(CALIPER_SOS_SOURCES
  Sos.cpp)

add_service_sources(${CALIPER_SOS_SOURCES})
add_caliper_service("sos CALIPER_HAVE_SOS")
