#
# - Find GEOPM
#
# GEOPM_INCLUDE_DIR - Path to geopm.h 
# GEOPM_LIBRARY     - Name of GEOPM library file
# GEOPM_FOUND       - True if GEOPM was found

#set(GEOPM_INSTALL "" CACHE PATH "GEOPM install directory")

#if(GEOPM_INSTALL)
    # install dir specified, only search them
    find_path(GEOPM_INCLUDE_DIR "geopm.h"
            NAMES geopm.h
            PATHS ${GEOPM_INSTALL} ${GEOPM_INSTALL}/include
            NO_DEFAULT_PATH
            )

    find_library(GEOPM_LIBRARY 
            NAMES geopm
            PATHS ${GEOPM_INSTALL} ${GEOPM_INSTALL}/lib
            NO_DEFAULT_PATH
            )
#endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GEOPM DEFAULT_MSG GEOPM_LIBRARY GEOPM_INCLUDE_DIR)

mark_as_advanced(GEOPM_LIBRARY GEOPM_INCLUDE_DIR)
