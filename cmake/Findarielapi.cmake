set(SST_ELEMENTS_ROOT
	"$ENV{SST_ELEMENTS_ROOT}"
	CACHE
	PATH
    "SST-Elements source directory")

set(SST_ELEMENTS_INSTALL
	"$ENV{SST_ELEMENTS_INSTALL}"
	CACHE
	PATH
    "SST-Elements install directory")

find_library(ARIELAPI_LIBRARY
	NAMES
	arielapi
	PATHS
	"${SST_ELEMENTS_INSTALL}/lib"
    "${SST_ELEMENTS_ROOT}/src/sst/elements/ariel/api")

if (ARIELAPI_LIBRARY)
	get_filename_component(ARIELAPI_LIBDIR ${ARIELAPI_LIBRARY} DIRECTORY)
endif ()

find_path(ARIELAPI_INCLUDE_DIR
	NAMES
	arielapi.h
	PATHS
	"${SST_ELEMENTS_INSTALL}/include/sst/elements/ariel/api" # Spack Location
    "${SST_ELEMENTS_ROOT}/src/sst/elements/ariel/api")

message(STATUS "checked in ${SST_ELEMENTS_INSTALL}/lib/sst-elements-library")

if ((NOT ARIELAPI_INCLUDE_DIR) OR (NOT ARIELAPI_LIBRARY))
	if ((NOT ARIELAPI_INCLUDE_DIR))
		message(SEND_ERROR "arielapi.h not found")
	endif ()
	if ((NOT ARIELAPI_LIBRARY))
		message(SEND_ERROR "libarielapi.so not found")
	endif ()
	message(FATAL_ERROR "Could not find part of the arielapi package. See prior message(s).")
else()
	set(ARIELAPI_FOUND TRUE CACHE BOOL "Whether the ArielAPI library and header files were found")
	set(ARIELAPI_INCLUDE_DIR "${ARIELAPI_INCLUDE_DIR}" CACHE STRING "The location of the Ariel API included with Ariel")
endif()
