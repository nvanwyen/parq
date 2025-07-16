#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)
find_package(OpenSSL REQUIRED)

#
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#
add_executable(parq
    parq.cpp
    reader.cpp
    b64.cpp
)

#
target_link_libraries(parq
    Arrow::arrow_shared
    Parquet::parquet_shared
    OpenSSL::SSL
    OpenSSL::Crypto
)

#
target_compile_options(parq PRIVATE -Wall -Wextra)
