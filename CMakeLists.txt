# 
# CMakeLists.txt
# ~~~~~~~~~~~~~~~~~~~~~
# 
# Copyright (c) 2004-2025 Metasystems Technologies Inc. (MTI)
# All rights reserved
# 
# Distributed under the MTI Software License, Version 0.1.
# 
# as defined by accompanying file MTI-LICENSE-0.1.info or
# at http://www.mtihq.com/license/MTI-LICENSE-0.1.info
# 

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
# NOTE: the VERSION script in the project root is the single source of truth --
# it echoes the value it keeps in source/VERSION. Always ask the script, never
# read source/VERSION directly, so there is exactly one place that decides what
# the version is. This also overrides the VERSION given to project() above.
set( PROJECT_VERSION "" )

execute_process( COMMAND "${PROJECT_DIRECTORY}/VERSION"
                 WORKING_DIRECTORY "${PROJECT_DIRECTORY}"
                 OUTPUT_VARIABLE PROJECT_VERSION
                 OUTPUT_STRIP_TRAILING_WHITESPACE
                 RESULT_VARIABLE VERSION_RESULT
                 ERROR_VARIABLE VERSION_ERROR )

if ( ( NOT VERSION_RESULT EQUAL 0 ) OR ( PROJECT_VERSION STREQUAL "" ) )
    message( FATAL_ERROR
        "Could not determine the version by running ${PROJECT_DIRECTORY}/VERSION\n"
        "  result: ${VERSION_RESULT}\n"
        "  error : ${VERSION_ERROR}\n"
        "Check that the file exists and is executable." )
endif()

# re-run cmake when the version changes, so the binary is never built with a
# stale name. These are change triggers only -- the value still comes from the
# script above.
set_property( DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
              "${PROJECT_DIRECTORY}/VERSION"
              "${PROJECT_DIRECTORY}/source/VERSION" )

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
