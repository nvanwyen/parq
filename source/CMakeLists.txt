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
find_package(Snappy REQUIRED)

#
include_directories(${PROJECT_DIRECTORY}/source)

#
add_executable(${PROJECT_NAME}
    parq.cpp
    reader.cpp
    b64.cpp
)

#
target_link_libraries(${PROJECT_NAME}
    Arrow::arrow_shared
    Parquet::parquet_shared
    OpenSSL::SSL
    OpenSSL::Crypto
    Snappy::snappy
)

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
