set(CALIPER_REPORT_SOURCES
    Report.cpp)

add_service_sources(${CALIPER_REPORT_SOURCES})
add_caliper_service("report")
