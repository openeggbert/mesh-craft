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

include(CTest)

if(BUILD_TESTING)
    add_executable(mcb_roundtrip_test
        test/mcb_roundtrip_test.cpp
    )
    target_link_libraries(mcb_roundtrip_test PRIVATE Mcb)
    add_test(NAME mcb_roundtrip COMMAND mcb_roundtrip_test)
    set_tests_properties(mcb_roundtrip PROPERTIES TIMEOUT 30)
endif()
