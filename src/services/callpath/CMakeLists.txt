include_directories(${LIBUNWIND_INCLUDE_DIR})

set(CALIPER_CALLPATH_SOURCES
    Callpath.cpp)

# install(TARGETS caliper-callpath DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(caliper-callpath OBJECT ${CALIPER_CALLPATH_SOURCES})

add_service_objlib("caliper-callpath")
add_caliper_service("callpath CALIPER_HAVE_LIBUNWIND")

# target_link_libraries(caliper ${LIBUNWIND_LIBRARY})
