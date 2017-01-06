
set(CALIPER_RECORDER_SOURCES
    Recorder.cpp)

# add_library(caliper-recorder ${CALIPER_RECORDER_SOURCES})

# target_link_libraries(caliper-recorder caliper-common)
# target_link_libraries(caliper-recorder caliper)

# install(TARGETS caliper-recorder DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_service_sources(${CALIPER_RECORDER_SOURCES})
add_caliper_service("recorder")
