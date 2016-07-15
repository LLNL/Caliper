set(CALIPER_TEXTLOG_SOURCES
    SnapshotTextFormatter.cpp
    TextLog.cpp)

add_service_sources(${CALIPER_TEXTLOG_SOURCES})
add_caliper_service("textlog")
