if(BUILD_TESTING)
  add_subdirectory(googletest)
endif()

if (WITH_GOTCHA)
  add_subdirectory(gotcha)
endif()

if (WITH_SOS)
  add_subdirectory(sos)
endif()

