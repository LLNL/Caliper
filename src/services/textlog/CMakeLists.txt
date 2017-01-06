set(CALIPER_TEXTLOG_SOURCES
    TextLog.cpp)

add_service_sources(${CALIPER_TEXTLOG_SOURCES})
add_caliper_service("textlog")
