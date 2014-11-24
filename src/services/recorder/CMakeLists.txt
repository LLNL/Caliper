include_directories("../../common")
include_directories("../../caliper")

set(CALIPER_RECORDER_SOURCES
    Recorder.cpp)

add_library(caliper-recorder ${CALIPER_RECORDER_SOURCES})

target_link_libraries(caliper-recorder caliper-common)
target_link_libraries(caliper-recorder caliper)

install(TARGETS caliper-recorder DESTINATION lib)
