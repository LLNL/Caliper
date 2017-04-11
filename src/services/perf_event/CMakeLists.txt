set(CALIPER_PERF_EVENT_SOURCES
    PerfEvent.cpp
    mmap_processor.cpp)

add_library(caliper-perf_event OBJECT ${CALIPER_PERF_EVENT_SOURCES})

add_service_objlib("caliper-perf_event")
add_caliper_service("perf_event CALIPER_HAVE_PERF_EVENT")
