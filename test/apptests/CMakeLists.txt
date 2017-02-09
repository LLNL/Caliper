file(GLOB apptest_python_files ${CMAKE_CURRENT_SOURCE_DIR}/*.py)
file(COPY ${apptest_python_files} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
