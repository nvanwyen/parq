# FindSnappy.cmake
#
# This module tries to find the Snappy compression library.
#
# Variables defined by this module:
#   Snappy_FOUND - True if Snappy was found
#   Snappy_INCLUDE_DIRS - Include directories
#   Snappy_LIBRARIES - Libraries to link
#   Snappy::snappy - Imported target

find_path(Snappy_INCLUDE_DIR
    NAMES snappy.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
        $ENV{SNAPPY_ROOT}/include
)

find_library(Snappy_LIBRARY
    NAMES snappy
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        $ENV{SNAPPY_ROOT}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Snappy
    REQUIRED_VARS Snappy_LIBRARY Snappy_INCLUDE_DIR
)

if(Snappy_FOUND)
    set(Snappy_INCLUDE_DIRS ${Snappy_INCLUDE_DIR})
    set(Snappy_LIBRARIES ${Snappy_LIBRARY})
    
    if(NOT TARGET Snappy::snappy)
        add_library(Snappy::snappy UNKNOWN IMPORTED)
        set_target_properties(Snappy::snappy PROPERTIES
            IMPORTED_LOCATION "${Snappy_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Snappy_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(Snappy_INCLUDE_DIR Snappy_LIBRARY)