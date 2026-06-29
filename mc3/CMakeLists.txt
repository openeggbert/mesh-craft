cmake_minimum_required(VERSION 3.21)
project(Mc3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# tinyxml2 — XML parser for mc3.xml I/O
# -----------------------------------------------------------------------------

include(FetchContent)

if(NOT TARGET tinyxml2 AND NOT TARGET tinyxml2::tinyxml2)
    FetchContent_Declare(
        tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG        10.0.0
    )
    set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tinyxml2)
endif()
# Alias so Mc3 can always link tinyxml2::tinyxml2 regardless of where the target came from
if(TARGET tinyxml2 AND NOT TARGET tinyxml2::tinyxml2)
    add_library(tinyxml2::tinyxml2 ALIAS tinyxml2)
endif()

# -----------------------------------------------------------------------------
# Mc3 — static library (MC3 model source format + XML I/O)
# -----------------------------------------------------------------------------

add_library(Mc3 STATIC
    src/Mc3Animation.cpp
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

# -----------------------------------------------------------------------------
# Round-trip unit tests
# -----------------------------------------------------------------------------

if(BUILD_TESTING)
    # When mc3 is built standalone (top-level project) nobody has called
    # enable_testing() yet, so add_test() would silently register nothing.
    # As a subdirectory of the root MeshCraft project, the root's include(CTest)
    # already enabled testing.
    if(PROJECT_IS_TOP_LEVEL)
        enable_testing()
    endif()

    add_executable(mc3_roundtrip_test test/roundtrip_test.cpp)
    target_link_libraries(mc3_roundtrip_test PRIVATE Mc3)

    # Pass features.mc3.xml path so the test can exercise the full parser
    # CMAKE_SOURCE_DIR is the top-level project root (may differ from mc3/ source dir)
    set(MC3_FEATURES_XML "${CMAKE_SOURCE_DIR}/test/features.mc3.xml")
    if(NOT EXISTS "${MC3_FEATURES_XML}")
        set(MC3_FEATURES_XML "")
    endif()

    add_test(NAME mc3_roundtrip
             COMMAND mc3_roundtrip_test ${MC3_FEATURES_XML})
    set_tests_properties(mc3_roundtrip PROPERTIES TIMEOUT 15)

    # editor_commands_test pulls in EditorAlgorithms.hpp from the MeshCraft
    # editor sources, which live at the repo root (src/MeshCraft/) — outside
    # the mc3/ subtree. Only build it when those sources are available (i.e.
    # as part of the root MeshCraft project) so mc3 stays buildable standalone.
    set(MC3_EDITOR_SRC_DIR "${CMAKE_SOURCE_DIR}/src/MeshCraft")
    if(EXISTS "${MC3_EDITOR_SRC_DIR}/EditorAlgorithms.hpp")
        add_executable(mc3_commands_test test/editor_commands_test.cpp)
        target_include_directories(mc3_commands_test PRIVATE
            ${MC3_EDITOR_SRC_DIR}
        )
        target_link_libraries(mc3_commands_test PRIVATE Mc3)
        target_compile_features(mc3_commands_test PRIVATE cxx_std_23)
        add_test(NAME mc3_commands COMMAND mc3_commands_test)
        set_tests_properties(mc3_commands PROPERTIES TIMEOUT 15)
    else()
        message(STATUS "mc3: skipping mc3_commands_test — MeshCraft editor "
                       "sources not found at ${MC3_EDITOR_SRC_DIR} "
                       "(standalone mc3 build)")
    endif()
endif()
