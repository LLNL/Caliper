set(CALIPER_TOPDOWN_SOURCES
  IntelTopdown.cpp
  TopdownCalculator.cpp
  HaswellTopdown.cpp
  SapphireRapidsTopdown.cpp)

if (CALIPER_HAVE_ARCH STREQUAL "sapphirerapids")
  if (NOT EXISTS ${PAPI_PREFIX}/bin/papi_coponent_avail)
    message(WARNING "Cannot check if PAPI uses rdpmc. Note that the topdown service will not work correctly on Sapphire Rapids if rdpmc is NOT enabled. This will be fixed by a future version of PAPI.")
  else ()
    execute_process(
      COMMAND ${PAPI_PREFIX}/bin/papi_coponent_avail
      OUTPUT_VARIABLE CALIPER_TOPDOWN_PAPI_COMPONENTS
    )
    string(FIND ${CALIPER_TOPDOWN_PAPI_COMPONENTS} "Fast counter read (rdpmc): yes" CALIPER_TOPDOWN_PAPI_USES_RDPMC)
    if (CALIPER_TOPDOWN_PAPI_USES_RDPMC EQUAL "-1")
      message(WARNING "Detected that PAPI does not use rdpmc to read counters. The topdown service will not work correctly on Sapphire Rapids if rdpmc is NOT enabled. This will be fixed by a future version of PAPI.")
    endif ()
  endif()
endif ()

add_library(caliper-topdown OBJECT ${CALIPER_TOPDOWN_SOURCES})

add_service_objlib("caliper-topdown")
add_caliper_service("topdown CALIPER_HAVE_PAPI")
