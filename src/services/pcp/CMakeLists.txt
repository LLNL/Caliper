set(CALI_PCP_SOURCES
  Pcp.cpp
  PcpMemory.cpp)

add_service_sources(${CALI_PCP_SOURCES})

add_caliper_service("pcp CALIPER_HAVE_PCP")
add_caliper_service("pcp_memory CALIPER_HAVE_PCP")
