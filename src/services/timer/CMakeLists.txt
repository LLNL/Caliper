include_directories("../../common")
include_directories("../../caliper")

set(CALIPER_TIMER_SOURCES
    Timer.cpp)

add_library(caliper-timer ${CALIPER_TIMER_SOURCES})

target_link_libraries(caliper-timer caliper-common)
target_link_libraries(caliper-timer caliper)

install(TARGETS caliper-timer DESTINATION lib)
