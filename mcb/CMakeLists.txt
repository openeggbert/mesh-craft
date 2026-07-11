cmake_minimum_required(VERSION 3.21)
project(Mcb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------------
# Mc3 library (from parent repo)
# When included from the root CMakeLists the target already exists; this
# guard adds it as a subdirectory only for standalone `mcb` builds so the
# Mc3 headers/include dirs are available.
# --------------------------------------------------------------------------
if(NOT TARGET Mc3)
    get_filename_component(_mc3_dir "${CMAKE_CURRENT_SOURCE_DIR}/../mc3" ABSOLUTE)
    add_subdirectory("${_mc3_dir}" mc3_dep)
endif()

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

# AUD-054/AUD-055: Mcb (the binary reader/writer -- untrusted-input parsing
# surface) previously had no warning flags at all.
target_compile_options(Mcb PRIVATE -Wall -Wextra)
if(COMMAND meshcraft_apply_sanitize)
    meshcraft_apply_sanitize(Mcb)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(mcb_roundtrip_test
        test/mcb_roundtrip_test.cpp
    )
    target_link_libraries(mcb_roundtrip_test PRIVATE Mcb)
    add_test(NAME mcb_roundtrip COMMAND mcb_roundtrip_test)
    set_tests_properties(mcb_roundtrip PROPERTIES TIMEOUT 30 LABELS "format")

    # SYS-W1-04 / SYS-W6-02: MCB corruption coverage beyond mcb_roundtrip_test
    # -- a byte-offset truncation sweep over a rich multi-section document,
    # and genuinely random (not just structured all-zeros) byte-noise fuzzing.
    add_executable(mcb_corruption_test
        test/mcb_corruption_test.cpp
    )
    target_link_libraries(mcb_corruption_test PRIVATE Mcb)
    add_test(NAME mcb_corruption COMMAND mcb_corruption_test)
    set_tests_properties(mcb_corruption PROPERTIES TIMEOUT 30 LABELS "format")

    # SYS-W1-01: Mc3Validation diagnostics for the MCB reader -- proves the
    # structured warning/error surface itself is populated (not just that
    # clamped/rejected VALUES are correct, already covered by
    # mcb_roundtrip's AUD-015/AUD-017 tests).
    add_executable(mcb_validation_test
        test/mcb_validation_test.cpp
    )
    target_link_libraries(mcb_validation_test PRIVATE Mcb)
    add_test(NAME mcb_validation COMMAND mcb_validation_test)
    set_tests_properties(mcb_validation PROPERTIES TIMEOUT 15 LABELS "format")
endif()
