set(CALIPER_REPORT_SOURCES
    Report.cpp)

add_library(caliper-report OBJECT ${CALIPER_REPORT_SOURCES})

add_service_objlib("caliper-report")
add_caliper_service("report")
