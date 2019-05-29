if(BUILD_TESTING)
  add_subdirectory(googletest)
endif()

if (WITH_GOTCHA AND NOT USE_EXTERNAL_GOTCHA)
  add_subdirectory(gotcha/src)
endif()

