include_directories("../../common")
include_directories("../../caliper")

include_directories(${LIBUNWIND_INCLUDE_DIR})

set(CALIPER_CALLPATH_SOURCES
    Callpath.cpp)

add_library(caliper-callpath ${CALIPER_CALLPATH_SOURCES})

target_link_libraries(caliper-callpath ${LIBUNWIND_LIBRARY})
target_link_libraries(caliper-callpath caliper-common)
target_link_libraries(caliper-callpath caliper)

install(TARGETS caliper-callpath DESTINATION lib)
