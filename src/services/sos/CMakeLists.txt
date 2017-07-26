set(CALIPER_TEXTLOG_SOURCES
  Sos.cpp)

add_service_sources(${CALIPER_TEXTLOG_SOURCES})
add_caliper_service("sos CALIPER_HAVE_SOS")
