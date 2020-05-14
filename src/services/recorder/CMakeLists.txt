set(CALIPER_RECORDER_SOURCES
    Recorder.cpp)

add_service_sources(${CALIPER_RECORDER_SOURCES})
add_caliper_service("recorder")
