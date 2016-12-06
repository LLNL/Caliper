include_directories(${PAPI_INCLUDE_DIRS})

set(CALIPER_PAPI_SOURCES
    Papi.cpp)

# add_library(caliper-papi ${CALIPER_PAPI_SOURCES})

# target_link_libraries(caliper-papi caliper-common)
# target_link_libraries(caliper-papi caliper)
# target_link_libraries(caliper-papi ${PAPI_LIBRARIES})

# install(TARGETS caliper-papi DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(caliper-papi OBJECT ${CALIPER_PAPI_SOURCES})

add_service_objlib("caliper-papi")
add_caliper_service("papi CALIPER_HAVE_PAPI")

# target_link_libraries(caliper ${PAPI_LIBRARIES})
