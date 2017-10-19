# - Find Dwarf
# Find the dwarf.h header from elf utils
#
#  DWARF_INCLUDE_DIR - where to find dwarf.h, etc.
#  DWARF_LIBRARIES   - List of libraries when using elf utils.
#  DWARF_FOUND       - True if fdo found.
if (DWARF_INCLUDE_DIR AND LIBDW_INCLUDE_DIR AND DWARF_LIBRARY AND ELF_LIBRARY)
    # Already in cache, be silent
    set(DWARF_FIND_QUIETLY TRUE)
endif (DWARF_INCLUDE_DIR AND LIBDW_INCLUDE_DIR AND DWARF_LIBRARY AND ELF_LIBRARY)
find_path(DWARF_INCLUDE_DIR dwarf.h
        /usr/include
        /usr/local/include
        /usr/include/libdwarf
        ~/usr/local/include
        )
find_path(LIBDW_INCLUDE_DIR elfutils/libdw.h
        /usr/include
        /usr/local/include
        ~/usr/local/include
        )
find_library(DWARF_LIBRARY
        NAMES dw dwarf
        PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 ~/usr/local/lib ~/usr/local/lib64
        )
find_library(ELF_LIBRARY
        NAMES elf
        PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 ~/usr/local/lib ~/usr/local/lib64
        )
find_library(EBL_LIBRARY
        NAMES ebl
        PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 ~/usr/local/lib ~/usr/local/lib64
        )
if (DWARF_INCLUDE_DIR AND LIBDW_INCLUDE_DIR AND DWARF_LIBRARY AND ELF_LIBRARY AND EBL_LIBRARY)
    set(DWARF_FOUND TRUE)
    set(DWARF_LIBRARIES ${DWARF_LIBRARY} ${ELF_LIBRARY} ${EBL_LIBRARY})
else (DWARF_INCLUDE_DIR AND LIBDW_INCLUDE_DIR AND DWARF_LIBRARY AND ELF_LIBRARY AND EBL_LIBRARY)
    set(DWARF_FOUND FALSE)
    set(DWARF_LIBRARIES)
endif (DWARF_INCLUDE_DIR AND LIBDW_INCLUDE_DIR AND DWARF_LIBRARY AND ELF_LIBRARY AND EBL_LIBRARY)
if (DWARF_FOUND)
    if (NOT DWARF_FIND_QUIETLY)
        message(STATUS "Found dwarf.h header: ${DWARF_INCLUDE_DIR}")
        message(STATUS "Found elfutils/libdw.h header: ${LIBDW_INCLUDE_DIR}")
        message(STATUS "Found libdw library: ${DWARF_LIBRARY}")
        message(STATUS "Found libelf library: ${ELF_LIBRARY}")
        message(STATUS "Found libebl library: ${EBL_LIBRARY}")
    endif (NOT DWARF_FIND_QUIETLY)
else (DWARF_FOUND)
    if (DWARF_FIND_REQUIRED)
        if (NOT DWARF_INCLUDE_DIR)
            message(FATAL_ERROR "Could NOT find dwarf include dir")
        endif (NOT DWARF_INCLUDE_DIR)
        if (NOT LIBDW_INCLUDE_DIR)
            message(FATAL_ERROR "Could NOT find libdw include dir")
        endif (NOT LIBDW_INCLUDE_DIR)
        if (NOT DWARF_LIBRARY)
            message(FATAL_ERROR "Could NOT find libdw library")
        endif (NOT DWARF_LIBRARY)
        if (NOT ELF_LIBRARY)
            message(FATAL_ERROR "Could NOT find libelf library")
        endif (NOT ELF_LIBRARY)
        if (NOT EBL_LIBRARY)
            message(FATAL_ERROR "Could NOT find libebl library")
        endif (NOT EBL_LIBRARY)
    endif (DWARF_FIND_REQUIRED)
endif (DWARF_FOUND)
mark_as_advanced(DWARF_INCLUDE_DIR LIBDW_INCLUDE_DIR DWARF_LIBRARY ELF_LIBRARY EBL_LIBRARY)