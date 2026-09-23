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

#
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)
find_package(OpenSSL REQUIRED)

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
        colormsg(HICYAN "Avro support enabled (avro-cpp ${avro-cpp_VERSION})")
    else()
        colormsg(HIRED "avro-cpp not found -- building without Avro support")
        colormsg(HIRED "  install it (brew install avro-cpp / dnf install avro-cpp-devel)")
        colormsg(HIRED "  or configure with -DWITH_AVRO=OFF to silence this")
    endif()
endif()

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
