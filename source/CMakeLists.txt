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

#
set(_app ${PROJECT_NAME}.${PROJECT_VERSION})
string(TOLOWER "${_app}" _app)

#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NOTE: QUIET here is deliberate, not cosmetic. Arrow's own Findlz4Alt.cmake and
# FindThriftAlt.cmake try config mode first and emit a multi-line CMake Warning
# whenever a dependency ships no <Pkg>Config.cmake. Homebrew's lz4 and thrift
# ship pkg-config files only, so that warning fires on every macOS configure --
# and the very next line resolves the dependency through pkg-config anyway. The
# warnings are alarming and say nothing actionable.
#
# Those Find modules forward QUIET to the inner find_package ( see the
# `if(lz4Alt_FIND_QUIETLY)` guard in each ), so asking quietly silences the
# false alarm at the source. Everything the noise was burying is printed back
# deliberately by the summary at the end of this file.
find_package(Arrow REQUIRED QUIET)
find_package(Parquet REQUIRED QUIET)
find_package(OpenSSL REQUIRED QUIET)

# Avro support is optional -- the reader is only compiled in when the avro-cpp
# library is present and WITH_AVRO is on. Without it the binary still builds
# and -a reports that this build has no avro support.
option(WITH_AVRO "Build with Apache Avro input support" ON)

set(AVRO_FOUND FALSE)

if(WITH_AVRO)
    find_package(avro-cpp CONFIG QUIET)

    if(avro-cpp_FOUND)
        set(AVRO_FOUND TRUE)

        # avro-cpp uses fmt in its public headers but does not export it as an
        # interface dependency, so link it here explicitly
        find_package(fmt QUIET)

        # The local-timestamp and nanosecond logical types only exist from
        # avro-cpp 1.12.2. There is no version macro a consumer can test, so
        # probe for them -- older avro-cpp (EL repos, 1.12.0) must still build.
        include(CheckCXXSourceCompiles)

        get_target_property(_avro_inc avro-cpp::avrocpp INTERFACE_INCLUDE_DIRECTORIES)
        set(CMAKE_REQUIRED_INCLUDES ${_avro_inc})
        set(CMAKE_REQUIRED_QUIET ON)

        check_cxx_source_compiles("
            #include <avro/LogicalType.hh>
            int main() {
                return (int) avro::LogicalType::LOCAL_TIMESTAMP_MICROS
                     + (int) avro::LogicalType::TIMESTAMP_NANOS;
            }" AVRO_HAS_EXTENDED_TIMESTAMPS)

        unset(CMAKE_REQUIRED_INCLUDES)
        unset(CMAKE_REQUIRED_QUIET)

        if(NOT AVRO_HAS_EXTENDED_TIMESTAMPS)
            colormsg(HIRED "avro-cpp predates local/nanosecond timestamps; those map to INT64")
        endif()
    else()
        colormsg(HIRED "avro-cpp not found -- building without Avro support")
        colormsg(HIRED "  install it (brew install avro-cpp / dnf install avro-cpp-devel)")
        colormsg(HIRED "  or configure with -DWITH_AVRO=OFF to silence this")
    endif()
endif()

# ORC support is optional, and unusually it is not a separate library: arrow
# carries the ORC adapter inside libarrow, but only when arrow itself was built
# with ARROW_ORC=ON. That is a property of the installed arrow, not something a
# consumer can ask for -- the ide7/ide8/ide9 images build arrow with it on,
# homebrew's apache-arrow ships it off -- so probe for it and degrade the same
# way avro does.
option(WITH_ORC "Build with Apache ORC input support" ON)

set(ORC_FOUND FALSE)

if(WITH_ORC)
    # find_package(Arrow) above pulls in arrow's own ArrowOptions.cmake, which
    # records every option arrow was configured with. ARROW_ORC from there is
    # the authoritative answer, so ask it rather than inferring one.
    #
    # NOTE: this used to be a check_cxx_source_compiles() that included the
    # adapter header, and that gave a false negative from arrow 26 onward.
    # Arrow 26 declares INTERFACE_COMPILE_FEATURES "cxx_std_20" and its headers
    # use `requires`. The parq target inherits that through Arrow::arrow_shared
    # and builds fine, but a bare compile check inherits nothing, so it ran at
    # CMAKE_CXX_STANDARD (17) and failed inside arrow/util/iterator.h -- nowhere
    # near ORC -- reporting an arrow that plainly had the adapter as lacking it.
    if(DEFINED ARROW_ORC)
        set(ORC_FOUND ${ARROW_ORC})
    else()
        # An arrow too old to export the option. The adapter header is only
        # installed when the adapter was built, so its presence is the next best
        # signal -- and looking for the file rather than compiling it keeps this
        # independent of whatever standard arrow's own headers need.
        get_target_property(_arrow_inc Arrow::arrow_shared INTERFACE_INCLUDE_DIRECTORIES)

        find_file(_arrow_orc_header
                  NAMES arrow/adapters/orc/adapter.h
                  HINTS ${_arrow_inc}
                  NO_CACHE)

        if(_arrow_orc_header)
            set(ORC_FOUND TRUE)
        endif()
    endif()

    if(NOT ORC_FOUND)
        colormsg(HIRED "this arrow was built without the ORC adapter -- building without ORC support")

        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            colormsg(HIRED "  homebrew's apache-arrow sets ARROW_ORC=OFF, so -o/--orc can never be")
            colormsg(HIRED "  available against it -- build arrow from source with -DARROW_ORC=ON")
        else()
            colormsg(HIRED "  install an arrow built with ARROW_ORC=ON (the ide7/ide8/ide9 images are)")
        endif()

        colormsg(HIRED "  or configure with -DWITH_ORC=OFF to silence this")
    endif()
endif()

# A short, deliberate summary of what this configure actually resolved.
#
# find_package(Arrow) drags arrow's whole transitive dependency graph in behind
# it -- aws-sdk, brotli, s2n, thrift, openssl -- each printing its own status
# lines, well over a hundred of them. The three facts that decide what this
# binary can do are trivial to lose in that scroll, and one of them fails
# silently: an arrow without the ORC adapter still configures and still builds,
# just without -o. Printing which arrow was resolved makes that obvious.
get_target_property(_arrow_lib Arrow::arrow_shared IMPORTED_LOCATION_RELEASE)

if(NOT _arrow_lib)
    get_target_property(_arrow_lib Arrow::arrow_shared IMPORTED_LOCATION)
endif()

if(AVRO_FOUND)
    set(_avro_state "yes ( avro-cpp ${avro-cpp_VERSION} )")
else()
    set(_avro_state "NO  ( -a/--avro will report no avro support )")
endif()

if(ORC_FOUND)
    set(_orc_state "yes ( adapter inside libarrow )")
else()
    set(_orc_state "NO  ( -o/--orc will report no orc support )")
endif()

message(STATUS "")
colormsg(HICYAN "parq ${PROJECT_VERSION} -- build configuration")
message(STATUS "  arrow    : ${ARROW_VERSION}")
message(STATUS "  library  : ${_arrow_lib}")
message(STATUS "  parquet  : ${PARQUET_VERSION} ( always available )")
message(STATUS "  avro     : ${_avro_state}")
message(STATUS "  orc      : ${_orc_state}")
message(STATUS "")

# NOTE: deliberately no include_directories(${PROJECT_DIRECTORY}/source) here.
# Project headers are included with quotes and sit beside the sources, so they
# resolve without it -- and putting source/ on the include path makes the
# standard <version> header resolve to source/VERSION on case-insensitive
# filesystems (macOS), which breaks every translation unit that includes
# <iostream>.

#
add_executable(${PROJECT_NAME}
    parq.cpp
    reader.cpp
    reader_parquet.cpp
    format.cpp
    b64.cpp
)

#
if(AVRO_FOUND)
    target_sources(${PROJECT_NAME} PRIVATE reader_avro.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_AVRO)

    if(AVRO_HAS_EXTENDED_TIMESTAMPS)
        target_compile_definitions(${PROJECT_NAME} PRIVATE AVRO_HAS_EXTENDED_TIMESTAMPS)
    endif()
endif()

# NOTE: no extra library to link -- the ORC adapter lives inside libarrow
# itself, which is already linked below.
if(ORC_FOUND)
    target_sources(${PROJECT_NAME} PRIVATE reader_orc.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_ORC)
endif()

#
# NOTE: Snappy and OpenSSL::SSL were linked here but never used -- arrow
# handles snappy internally and only libcrypto (EVP) is called, for the file
# checksum. Requiring them blocked builds for nothing.
target_link_libraries(${PROJECT_NAME}
    Arrow::arrow_shared
    Parquet::parquet_shared
    OpenSSL::Crypto
)

#
if(AVRO_FOUND)
    target_link_libraries(${PROJECT_NAME} avro-cpp::avrocpp)

    if(fmt_FOUND)
        target_link_libraries(${PROJECT_NAME} fmt::fmt)
    endif()
endif()

#
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)

#
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_app})

#
file(MAKE_DIRECTORY "${PROJECT_HOME}/")
file(MAKE_DIRECTORY "${PROJECT_HOME}/bin/")
file(MAKE_DIRECTORY "${PROJECT_HOME}/lib/")

#
set(PROJECT_WRAPPER ${PROJECT_HOME}/${PROJECT_NAME})
string(TOLOWER "${PROJECT_WRAPPER}" PROJECT_WRAPPER)
#
file(WRITE "${PROJECT_WRAPPER}"
    "#!/bin/bash\n"
    "\n"
    "#\n"
    "# parq (wrapper)\n"
    "# ~~~~~~~~~~~~~~~~~~~~~\n"
    "#\n"
    "# Copyright (c) 2004-2025 Metasystems Technologies Inc. (MTI)\n"
    "# All rights reserved\n"
    "#\n"
    "# Distributed under the MTI Software License, Version 0.1.\n"
    "#\n"
    "# as defined by accompanying file MTI-LICENSE-0.1.info or\n"
    "# at http://www.mtihq.com/license/MTI-LICENSE-0.1.info\n"
    "#\n"
    "\n"
    "dir=\"\$(cd \"\$(dirname \"\$0\")\" ; pwd -P)\"\n"
    "LD_LIBRARY_PATH=\${dir}/lib:\${LD_LIBRARY_PATH}:.: && \${dir}/bin/parq \${@}")
#
file(CHMOD "${PROJECT_WRAPPER}" PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE)

#
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND echo "[ .. ] Creating symbloic link ${PROJECT_NAME} for ${_app}"
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${_app} ${PROJECT_NAME})

#
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND echo "[ .. ] Copying ${_app} to ${PROJECT_HOME}/bin/"
    COMMAND ${CMAKE_COMMAND} -E copy ${_app} ${PROJECT_HOME}/bin/${_app})

#
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND echo "[ .. ] Moving ${PROJECT_NAME} to ${PROJECT_HOME}/bin/"
    COMMAND ${CMAKE_COMMAND} -E rename ${PROJECT_NAME} ${PROJECT_HOME}/bin/${PROJECT_NAME})
