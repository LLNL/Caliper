include_directories ("../../common")
include_directories ("../../reader")
include_directories ("../util")
include_directories ("../../caliper")

set(CALIPER_QUERY_COMMON_SOURCES
  query_common.cpp)

set(CALIPER_QUERY_SOURCES
  AttributeExtract.cpp
  cali-query.cpp)

add_library(query-common OBJECT ${CALIPER_QUERY_COMMON_SOURCES})

add_executable(cali-query
  $<TARGET_OBJECTS:query-common>
  ${CALIPER_QUERY_SOURCES})

target_link_libraries(cali-query caliper-tools-util caliper-serial)
target_link_libraries(cali-query Threads::Threads)

install(TARGETS cali-query DESTINATION ${CMAKE_INSTALL_BINDIR})
