add_caliper_option(WITH_PAPI_RDPMC "Declare that PAPI is built to use rdpmc for reading counters" TRUE)

set(CALIPER_TOPDOWN_SOURCES
  IntelTopdown.cpp
  TopdownCalculator.cpp
  HaswellTopdown.cpp)


if (WITH_PAPI_RDPMC)
  if (CALIPER_HAVE_ARCH STREQUAL "sapphirerapids")
    message(WARNING "Trying to use rdpmc for topdown on Sapphire Rapids will likely result in invalid values!")
  endif()
  list(APPEND CALIPER_TOPDOWN_SOURCES SapphireRapidsTopdown_rdpmc.cpp)
else()
  list(APPEND CALIPER_TOPDOWN_SOURCES SapphireRapidsTopdown_read.cpp)
endif ()

add_library(caliper-topdown OBJECT ${CALIPER_TOPDOWN_SOURCES})

add_service_objlib("caliper-topdown")
add_caliper_service("topdown CALIPER_HAVE_PAPI")
