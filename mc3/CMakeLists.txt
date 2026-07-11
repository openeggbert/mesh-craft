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

target_compile_options(Mc3 PRIVATE -Wall -Wextra)

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
    set_tests_properties(mc3_roundtrip PROPERTIES TIMEOUT 15 LABELS "format")

    # Hostile-numeric-input test: NaN/Inf/out-of-range attributes must not
    # survive parsing into the model (finiteOr sanitization).
    add_executable(mc3_finite_input_test test/finite_input_test.cpp)
    target_link_libraries(mc3_finite_input_test PRIVATE Mc3)
    add_test(NAME mc3_finite_input COMMAND mc3_finite_input_test)
    set_tests_properties(mc3_finite_input PROPERTIES TIMEOUT 15 LABELS "format")

    # Input-budget test: tessellation counts clamped to a safe upper bound so a
    # hostile <sphere segments="100000000"/> can't drive unbounded allocation.
    add_executable(mc3_input_budget_test test/input_budget_test.cpp)
    target_link_libraries(mc3_input_budget_test PRIVATE Mc3)
    add_test(NAME mc3_input_budget COMMAND mc3_input_budget_test)
    set_tests_properties(mc3_input_budget PROPERTIES TIMEOUT 15 LABELS "format")

    # editor_commands_test pulls in MeshCraft/EditorAlgorithms.hpp from the
    # MeshCraft editor's public include dir, which lives at the repo root
    # (include/) — outside the mc3/ subtree. Only build it when those
    # headers are available (i.e. as part of the root MeshCraft project)
    # so mc3 stays buildable standalone.
    set(MC3_EDITOR_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
    if(EXISTS "${MC3_EDITOR_INCLUDE_DIR}/MeshCraft/EditorAlgorithms.hpp")
        add_executable(mc3_commands_test
            test/editor_commands_test.cpp
            "${CMAKE_SOURCE_DIR}/src/MeshCraft/Editor/SelectionManager.cpp"
        )
        target_include_directories(mc3_commands_test PRIVATE
            ${MC3_EDITOR_INCLUDE_DIR}
        )
        target_link_libraries(mc3_commands_test PRIVATE Mc3)
        target_compile_features(mc3_commands_test PRIVATE cxx_std_23)
        add_test(NAME mc3_commands COMMAND mc3_commands_test)
        set_tests_properties(mc3_commands PROPERTIES TIMEOUT 15 LABELS "commands")
    else()
        message(STATUS "mc3: skipping mc3_commands_test — MeshCraft editor "
                       "headers not found at ${MC3_EDITOR_INCLUDE_DIR} "
                       "(standalone mc3 build)")
    endif()
endif()
