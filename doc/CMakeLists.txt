project(caliper-doc)

find_package(Doxygen)
find_package(Sphinx)

if (DOXYGEN_FOUND)
  add_subdirectory(doxygen)
endif()
if(SPHINX_FOUND)
  add_subdirectory(sphinx)
endif()
