include_directories("../common")

set(CALIPER_SERVICES_SOURCES
    Services.cpp)

add_library(caliper-services ${CALIPER_SERVICES_SOURCES})

target_link_libraries(caliper-services caliper-csv)
target_link_libraries(caliper-services caliper-timer)
target_link_libraries(caliper-services caliper-common)

install(TARGETS caliper-services DESTINATION lib)

add_subdirectory(csv)
add_subdirectory(timer)
