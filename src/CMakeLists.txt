if (${CMAKE_CXX_COMPILER_ID} MATCHES Intel)
  # CMake does not have proper compiler feature support for Intel compilers :-/
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
else()
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_STANDARD_REQUIRED ON) 

  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_C_STANDARD   99)
endif()

include_directories(${PROJECT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(common)
add_subdirectory(reader)
add_subdirectory(services)
add_subdirectory(caliper)
add_subdirectory(caliper-stub)

if (CALIPER_HAVE_MPI)
  add_subdirectory(mpi-common)
  add_subdirectory(mpi-rt)
endif()

# Make the combined libcaliper
add_library(caliper ${CALIPER_ALL_OBJS})

set_target_properties(caliper PROPERTIES SOVERSION ${CALIPER_MAJOR_VERSION})
set_target_properties(caliper PROPERTIES VERSION ${CALIPER_VERSION})

foreach (_extlib ${CALIPER_ALL_EXTERNAL_LIBS})
  target_link_libraries(caliper PRIVATE ${_extlib})
endforeach()

if (WITH_TOOLS)
  add_subdirectory(tools)
endif()
if (WITH_FORTRAN)
  add_subdirectory(fortran)
endif()

install(TARGETS caliper
  EXPORT caliper
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
