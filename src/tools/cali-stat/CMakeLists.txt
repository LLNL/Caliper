set(CALIPER_STAT_SOURCES
  cali-stat.cpp)

add_executable(cali-stat ${CALIPER_STAT_SOURCES})

target_link_libraries(cali-stat caliper-tools-util caliper-serial)

install(TARGETS cali-stat DESTINATION ${CMAKE_INSTALL_BINDIR})
