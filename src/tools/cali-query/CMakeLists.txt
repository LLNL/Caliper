set(CALIPER_QUERY_COMMON_SOURCES
  query_common.cpp)

set(CALIPER_QUERY_SOURCES
  AttributeExtract.cpp
  cali-query.cpp)

add_library(query-common OBJECT ${CALIPER_QUERY_COMMON_SOURCES})
target_compile_features(query-common PUBLIC cxx_std_11)

add_executable(cali-query
  $<TARGET_OBJECTS:query-common>
  $<TARGET_OBJECTS:caliper-tools-util>
  ${CALIPER_QUERY_SOURCES})

target_link_libraries(cali-query caliper)
target_link_libraries(cali-query Threads::Threads)

install(TARGETS cali-query DESTINATION ${CMAKE_INSTALL_BINDIR})
