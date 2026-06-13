cmake_minimum_required(VERSION 3.21)
project(mc3togltf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --------------------------------------------------------------------------
# Mc3 library (from parent repo)
# --------------------------------------------------------------------------
if(NOT TARGET Mc3)
    get_filename_component(_mc3_dir "${CMAKE_CURRENT_SOURCE_DIR}/../mc3" ABSOLUTE)
    add_subdirectory("${_mc3_dir}" mc3_dep)
endif()

# --------------------------------------------------------------------------
# tinyxml2 — lightweight XML parser
# --------------------------------------------------------------------------
FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG        10.0.0
)
set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinyxml2)

# --------------------------------------------------------------------------
# tinygltf — header-only glTF 2.0 reader/writer
# --------------------------------------------------------------------------
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY          ON  CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL              OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

# --------------------------------------------------------------------------
# tinyobjloader — header-only OBJ mesh loader
# --------------------------------------------------------------------------
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
)
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinyobjloader)

# --------------------------------------------------------------------------
# Executable
# --------------------------------------------------------------------------
add_executable(mc3togltf
    src/main.cpp
    src/Mc3XmlParser.cpp
    src/MeshBuilder.cpp
    src/GltfExporter.cpp
)

target_include_directories(mc3togltf PRIVATE
    src
    ${tinygltf_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
)

target_link_libraries(mc3togltf PRIVATE
    Mc3
    tinyxml2::tinyxml2
    tinyobjloader
)

install(TARGETS mc3togltf RUNTIME DESTINATION bin)

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
include(CTest)

if(BUILD_TESTING)
    find_program(PYTHON3_EXEC python3)
    if(PYTHON3_EXEC)
        set(_anim_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/animation_test.mc3.xml")
        set(_house_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/house.mc3.xml")
        if(EXISTS "${_anim_xml}" AND EXISTS "${_house_xml}")
            add_test(
                NAME mc3togltf_gltf
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/gltf_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_anim_xml}"
                        "${_house_xml}"
            )
            set_tests_properties(mc3togltf_gltf PROPERTIES TIMEOUT 30)
        endif()
    endif()
endif()
