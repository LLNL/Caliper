include_directories("../common/")
include_directories("../reader/")

include_directories(${PYTHON_INCLUDE_DIRS})

add_library(calireader SHARED calireader.cpp)

target_link_libraries(calireader caliper-reader ${PYTHON_LIBRARIES})

#file(COPY calireader.py DESTINATION ".")
