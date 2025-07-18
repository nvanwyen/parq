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
