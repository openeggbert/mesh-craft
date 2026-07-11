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
endif()
