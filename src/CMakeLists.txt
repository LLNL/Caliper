set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD   99)

include(CheckCXXCompilerFlag)
set(Wall_flag)
check_cxx_compiler_flag("-Wall" Supports_Wall_Flag)
if(Supports_Wall_Flag)
  set(Wall_flag "-Wall")
endif()

include_directories(${PROJECT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/interface/c_fortran)

add_subdirectory(common)
add_subdirectory(interface/c_fortran)
add_subdirectory(reader)
add_subdirectory(services)
add_subdirectory(caliper)

if (CALIPER_HAVE_MPI)
  add_subdirectory(mpi)
endif()

set(CALIPER_COMMON_OBJS
  $<TARGET_OBJECTS:caliper-common>
  $<TARGET_OBJECTS:caliper-interface>
  $<TARGET_OBJECTS:caliper-reader>
  $<TARGET_OBJECTS:caliper-services>
  $<TARGET_OBJECTS:caliper-runtime>
  ${CALIPER_SERVICES_LIBS}
  ${CALIPER_EXTERNAL_SOURCES})

list(APPEND CALIPER_SERIAL_OBJS
  ${CALIPER_COMMON_OBJS})
list(APPEND CALIPER_ALL_OBJS
  ${CALIPER_COMMON_OBJS})

# Make (non-mpi) libcaliper-serial

add_library(caliper-serial ${CALIPER_SERIAL_OBJS})

set_target_properties(caliper-serial PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper-serial PROPERTIES VERSION ${CALIPER_VERSION})

list(APPEND CALIPER_EXTERNAL_LIBS
  Threads::Threads)

foreach (_extlib ${CALIPER_EXTERNAL_LIBS})
  target_link_libraries(caliper-serial PRIVATE ${_extlib})
endforeach()

# Make the combined libcaliper

add_library(caliper ${CALIPER_ALL_OBJS})

set_target_properties(caliper PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper PROPERTIES VERSION ${CALIPER_VERSION})

list(APPEND CALIPER_ALL_EXTERNAL_LIBS
  ${CALIPER_EXTERNAL_LIBS})

foreach (_extlib ${CALIPER_ALL_EXTERNAL_LIBS})
  target_link_libraries(caliper PRIVATE ${_extlib})
endforeach()

if (WITH_TOOLS)
  add_subdirectory(tools)
endif()

install(TARGETS caliper-serial
  EXPORT caliper 
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(TARGETS caliper
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
