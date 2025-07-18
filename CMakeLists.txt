# requires cmake version 2.16 or greater
# https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html
cmake_minimum_required( VERSION 3.24 )

# project
project( parq
         VERSION 1.0.0.2
         DESCRIPTION "Parquet file reader"
         LANGUAGES CXX )

if ( NOT CMAKE_VERBOSE_MAKEFILE )
    set( CMAKE_VERBOSE_MAKEFILE false )
endif()

#
set( PROJECT_NAME parq )

#
set( PROJECT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
set( PROJECT_HOME ${PROJECT_DIRECTORY}/home )
set( CMAKE_MODULE_PATH ${PROJECT_DIRECTORY}/cmake )

# version
set( PROJECT_VERSION "" )
file( STRINGS "${PROJECT_DIRECTORY}/VERSION" PROJECT_VERSION )

#
include( Utilities )
include( MacroEnsureOutOfSourceBuild )

# ensure out-of-source builds ( error when not out-of-source )
EnsureOutOfSourceBuild( "
    *** IMPORTANT ***
    Builds can *ONLY* be out-of-source!
    Make sure to delete the ./CMakeCache.txt file *before* retrying
    You can use the supplied ./configure.sh script, to build the
    project with release, debug and in verbose modes use:
        ./configure.sh --help
    for a full list of options
    *** IMPORTANT ***
    " )

#
include(SelectLibraryConfigurations)

#
if ( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    colormsg( HIRED "Detected the ${CMAKE_SYSTEM_NAME} Operating System" )
else()
    if ( CMAKE_SYSTEM_NAME STREQUAL "Darwin" )
        colormsg( HIRED "Detected the ${CMAKE_SYSTEM_NAME} Operating System" )
    else()
        message( FATAL_ERROR "${CMAKE_SYSTEM_NAME} Operation System not currently supported" )
    endif()
endif()

#
add_subdirectory( source )
