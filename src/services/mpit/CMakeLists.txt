include_directories(${MPI_C_INCLUDE_PATH})

set(CALIPER_MPIT_SOURCES
    MpitService.cpp)

#add_service_sources(${CALIPER_MPIT_SOURCES})

#add_library(caliper-mpit ${CALIPER_MPIT_SOURCES})

#target_link_libraries(caliper-mpit caliper-common)
#target_link_libraries(caliper-mpit caliper)
#target_link_libraries(caliper-mpit ${MPI_C_LIBRARIES} ${MPI_CXX_LIBRARIES})

#install(TARGETS caliper-mpit
#  EXPORT caliper
#  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_library(caliper-mpit OBJECT ${CALIPER_MPIT_SOURCES})
add_service_objlib("caliper-mpit")

add_caliper_service("mpit CALIPER_HAVE_MPIT")
