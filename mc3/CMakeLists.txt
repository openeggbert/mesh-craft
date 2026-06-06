cmake_minimum_required(VERSION 3.21)
project(Mc3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# tinyxml2 — XML parser for mc3.xml I/O
# -----------------------------------------------------------------------------

include(FetchContent)

if(NOT TARGET tinyxml2::tinyxml2)
    FetchContent_Declare(
        tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG        10.0.0
    )
    set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tinyxml2)
endif()

# -----------------------------------------------------------------------------
# Mc3 — static library (MC3 model source format + XML I/O)
# -----------------------------------------------------------------------------

add_library(Mc3 STATIC
    src/Mc3Document.cpp
    src/Mc3Object.cpp
    src/Mc3Material.cpp
    src/Mc3Texture.cpp
    src/Mc3Primitive.cpp
    src/Mc3CsgOperation.cpp
    src/Mc3XmlParser.cpp
    src/Mc3XmlWriter.cpp
)

target_include_directories(Mc3
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Mc3 PRIVATE tinyxml2::tinyxml2)
