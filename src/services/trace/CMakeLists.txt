set(CALIPER_TRACE_SOURCES
    TraceBufferChunk.cpp
    Trace.cpp)

add_service_sources(${CALIPER_TRACE_SOURCES})
add_caliper_service("trace")
