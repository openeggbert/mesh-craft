cmake_minimum_required(VERSION 3.21)
project(Mcb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(Mcb STATIC
    src/McbWriter.cpp
    src/McbReader.cpp
)

target_include_directories(Mcb
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(Mcb PUBLIC Mc3)
