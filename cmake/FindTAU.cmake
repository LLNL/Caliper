# Find TAU
# ~~~~~~~~~~~~
# Copyright (c) 2017, Kevin Huck <khuck at cs.uoregon.edu>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for TAU library
#
# If it's found it sets TAU_FOUND to TRUE
# and following variables are set:
#    TAU_INCLUDE_DIR
#    TAU_LIBRARIES

# First, look in only one variable, TAU_DIR.  This script will accept any of:
# TAU_DIR, TAU_ROOT, TAU_DIR, TAU_ROOT, TAU_PREFIX, or environment variables
# using the same set of names.

if ("${TAU_DIR} " STREQUAL " ")
    IF (NOT TAU_FIND_QUIETLY)
        message("TAU_DIR not set, trying alternatives...")
    ENDIF (NOT TAU_FIND_QUIETLY)

    # All upper case options
    if (DEFINED TAU_PREFIX)
        set(TAU_DIR ${TAU_PREFIX})
    endif (DEFINED TAU_PREFIX)
    if (DEFINED TAU_DIR)
        set(TAU_DIR ${TAU_DIR})
    endif (DEFINED TAU_DIR)
    if (DEFINED TAU_ROOT)
        set(TAU_DIR ${TAU_ROOT})
    endif (DEFINED TAU_ROOT)
    if (DEFINED ENV{TAU_PREFIX})
        set(TAU_DIR $ENV{TAU_PREFIX})
    endif (DEFINED ENV{TAU_PREFIX})
    if (DEFINED ENV{TAU_DIR})
        set(TAU_DIR $ENV{TAU_DIR})
    endif (DEFINED ENV{TAU_DIR})
    if (DEFINED ENV{TAU_ROOT})
        set(TAU_DIR $ENV{TAU_ROOT})
    endif (DEFINED ENV{TAU_ROOT})
endif ("${TAU_DIR} " STREQUAL " ")

# Check to make sure the TAU_MAKEFILE is set, so we know 
# which TAU configuration to use

if (NOT DEFINED TAU_MAKEFILE)
    if (DEFINED ENV{TAU_MAKEFILE})
        set(TAU_MAKEFILE $ENV{TAU_MAKEFILE})
    else (DEFINED ENV{TAU_MAKEFILE})
        IF (NOT TAU_FIND_QUIETLY)
            MESSAGE(STATUS "TAU_MAKEFILE not set! Please set TAU_MAKEFILE to a valid TAU makefile.")
        ENDIF (NOT TAU_FIND_QUIETLY)
    endif (DEFINED ENV{TAU_MAKEFILE})
endif (NOT DEFINED TAU_MAKEFILE)

IF (NOT TAU_FIND_QUIETLY)
    MESSAGE(STATUS "TAU_DIR set to: '${TAU_DIR}'")
    MESSAGE(STATUS "TAU_MAKEFILE set to: '${TAU_MAKEFILE}'")
ENDIF (NOT TAU_FIND_QUIETLY)

# First, see if the archfind program is in the tau directory
# If so, use it.

if (NOT DEFINED TAU_ARCH)
    IF (NOT TAU_FIND_QUIETLY)
        message("FindTAU: looking for ${TAU_DIR}/utils/archfind")
    ENDIF (NOT TAU_FIND_QUIETLY)

    find_program (TAU_ARCHFIND NAMES utils/archfind
                PATHS 
                "${TAU_DIR}"
                NO_DEFAULT_PATH)

    if(TAU_ARCHFIND)
        IF (NOT TAU_FIND_QUIETLY)
            message("FindTAU: run ${TAU_ARCHFIND}")
        ENDIF (NOT TAU_FIND_QUIETLY)
        execute_process(COMMAND ${TAU_ARCHFIND}
            OUTPUT_VARIABLE TAU_ARCH
            RESULT_VARIABLE TAU_arch_ret
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        IF (NOT TAU_FIND_QUIETLY)
            message("FindTAU: return value = ${TAU_arch_ret}")
            message("FindTAU: output = ${TAU_ARCH}")
        ENDIF (NOT TAU_FIND_QUIETLY)
    endif(TAU_ARCHFIND)
endif (NOT DEFINED TAU_ARCH)

# Second, see if the tau_cxx.sh program is in our path.  
# If so, use it.

IF (NOT TAU_FIND_QUIETLY)
    message("FindTAU: looking for tau_cxx.sh")
ENDIF (NOT TAU_FIND_QUIETLY)

find_program (TAU_CONFIG NAMES tau_cxx.sh 
              PATHS 
              "${TAU_DIR}/${TAU_ARCH}/bin"
              ENV PATH
              NO_DEFAULT_PATH)

if(TAU_CONFIG)
    IF (NOT TAU_FIND_QUIETLY)
        message("FindTAU: run ${TAU_CONFIG}")
    ENDIF (NOT TAU_FIND_QUIETLY)
    execute_process(COMMAND ${TAU_CONFIG}
        "-tau_makefile=${TAU_MAKEFILE}"
        "-tau:showlibs"
        OUTPUT_VARIABLE TAU_config_out
        RESULT_VARIABLE TAU_config_ret
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    IF (NOT TAU_FIND_QUIETLY)
        message("FindTAU: return value = ${TAU_config_ret}")
        message("FindTAU: output = ${TAU_config_out}")
    ENDIF (NOT TAU_FIND_QUIETLY)
    if(TAU_config_ret EQUAL 0)
        string(REPLACE " " ";" TAU_config_list ${TAU_config_out})
        IF (NOT TAU_FIND_QUIETLY)
            message("FindTAU: list = ${TAU_config_list}")
        ENDIF (NOT TAU_FIND_QUIETLY)
        set(TAU_libs)
        set(TAU_lib_hints)
        set(TAU_lib_flags)
        foreach(OPT IN LISTS TAU_config_list)
            if(OPT MATCHES "^-L(.*)")
                list(APPEND TAU_lib_hints "${CMAKE_MATCH_1}")
            elseif(OPT MATCHES "^-l(.*)")
                list(APPEND TAU_libs "${CMAKE_MATCH_1}")
            #else()
                #list(APPEND TAU_libs "${OPT}")
            endif()
        endforeach()
        set(HAVE_TAU 1)
    endif()
    IF (NOT TAU_FIND_QUIETLY)
        message("FindTAU: hints = ${TAU_lib_hints}")
        message("FindTAU: libs = ${TAU_libs}")
        message("FindTAU: flags = ${TAU_lib_flags}")
    ENDIF (NOT TAU_FIND_QUIETLY)
    set(TAU_LIBRARIES)
    foreach(lib IN LISTS TAU_libs)
      find_library(TAU_${lib}_LIBRARY NAME ${lib} HINTS ${TAU_lib_hints})
      if(TAU_${lib}_LIBRARY)
        list(APPEND TAU_LIBRARIES ${TAU_${lib}_LIBRARY})
        IF (NOT TAU_FIND_QUIETLY)
            message("Library: ${TAU_${lib}_LIBRARY}")
        endif (NOT TAU_FIND_QUIETLY)
      else()
        list(APPEND TAU_LIBRARIES ${lib})
      endif()
    endforeach()
    set(HAVE_TAU 1)
    set(TAU_LIBRARIES "${TAU_LIBRARIES}" CACHE STRING "")
    FIND_PATH(TAU_INCLUDE_DIR TAU.h "${TAU_DIR}/include" NO_DEFAULT_PATH)

else(TAU_CONFIG)

    find_package(PkgConfig REQUIRED)
    pkg_search_module(TAU REQUIRED libTAU QUIET)
    # could be needed on some platforms
    pkg_search_module(FABRIC libfabric QUIET)

    if(NOT TAU_FOUND)

    # FIND_PATH and FIND_LIBRARY normally search standard locations
    # before the specified paths. To search non-standard paths first,
    # FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
    # and then again with no specified paths to search the default
    # locations. When an earlier FIND_* succeeds, subsequent FIND_*s
    # searching for the same item do nothing. 

    FIND_PATH(TAU_INCLUDE_DIR TAU.h 
        "${TAU_DIR}/include" NO_DEFAULT_PATH)
    FIND_PATH(TAU_INCLUDE_DIR TAU.h)

    FIND_LIBRARY(TAU_LIBRARIES NAMES libTAU.a PATHS
        "${TAU_DIR}/lib" NO_DEFAULT_PATH)
    FIND_LIBRARY(TAU_LIBRARIES NAMES TAU)

    endif()
    
endif(TAU_CONFIG)

IF (TAU_INCLUDE_DIR AND TAU_LIBRARIES)
   SET(TAU_FOUND TRUE)
ENDIF (TAU_INCLUDE_DIR AND TAU_LIBRARIES)

IF (TAU_FOUND)

   IF (NOT TAU_FIND_QUIETLY)
      MESSAGE(STATUS "Found TAU: ${TAU_LIBRARIES}")
   ENDIF (NOT TAU_FIND_QUIETLY)

ELSE (TAU_FOUND)

   IF (TAU_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find TAU")
   ENDIF (TAU_FIND_REQUIRED)

ENDIF (TAU_FOUND)
