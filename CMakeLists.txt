cmake_minimum_required(VERSION 3.21)
project(MeshCraft LANGUAGES C CXX)

include(CTest)

message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

if (NOT WIN32 AND CMAKE_BINARY_DIR MATCHES "build-windows")
    message(WARNING "Building in 'build-windows' directory, but WIN32 is not set. "
            "Make sure you used -DCMAKE_TOOLCHAIN_FILE=<toolchain> on a CLEAN build directory.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Graphics backend selection
# -----------------------------------------------------------------------------
#
# Override with:
#   cmake -S . -B build -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER
#   cmake -S . -B build -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL
#
# Supported: SDL_RENDERER, EASYGL, BGFX, VULKAN
# -----------------------------------------------------------------------------

set(MESH_CRAFT_GRAPHICS_BACKEND "EASYGL" CACHE STRING "MeshCraft graphics backend")
set_property(CACHE MESH_CRAFT_GRAPHICS_BACKEND PROPERTY STRINGS
        SDL_RENDERER EASYGL BGFX VULKAN)

string(TOUPPER "${MESH_CRAFT_GRAPHICS_BACKEND}" MESH_CRAFT_GRAPHICS_BACKEND_UPPER)

if(ANDROID OR EMSCRIPTEN)
    set(MESH_CRAFT_GRAPHICS_BACKEND_UPPER "SDL_RENDERER")
endif()

if(NOT MESH_CRAFT_GRAPHICS_BACKEND_UPPER MATCHES "^(SDL_RENDERER|EASYGL|BGFX|VULKAN)$")
    message(FATAL_ERROR
            "Unsupported MESH_CRAFT_GRAPHICS_BACKEND='${MESH_CRAFT_GRAPHICS_BACKEND}'. "
            "Use SDL_RENDERER, EASYGL, BGFX, or VULKAN.")
endif()

message(STATUS "MeshCraft graphics backend: ${MESH_CRAFT_GRAPHICS_BACKEND_UPPER}")

if(MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "SDL_RENDERER")
    set(CNA_BACKEND_SDL_RENDERER ON  CACHE BOOL "" FORCE)
    set(CNA_BACKEND_EASY_GL      OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_BGFX         OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_VULKAN       OFF CACHE BOOL "" FORCE)
    set(CNA_GRAPHICS_BACKEND "SDL_RENDERER" CACHE STRING "" FORCE)
elseif(MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "EASYGL")
    set(CNA_BACKEND_SDL_RENDERER OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_EASY_GL      ON  CACHE BOOL "" FORCE)
    set(CNA_BACKEND_BGFX         OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_VULKAN       OFF CACHE BOOL "" FORCE)
    set(CNA_GRAPHICS_BACKEND "EASYGL" CACHE STRING "" FORCE)
elseif(MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "BGFX")
    set(CNA_BACKEND_SDL_RENDERER OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_EASY_GL      OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_BGFX         ON  CACHE BOOL "" FORCE)
    set(CNA_BACKEND_VULKAN       OFF CACHE BOOL "" FORCE)
    set(CNA_GRAPHICS_BACKEND "BGFX" CACHE STRING "" FORCE)
elseif(MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "VULKAN")
    set(CNA_BACKEND_SDL_RENDERER OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_EASY_GL      OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_BGFX         OFF CACHE BOOL "" FORCE)
    set(CNA_BACKEND_VULKAN       ON  CACHE BOOL "" FORCE)
    set(CNA_GRAPHICS_BACKEND "VULKAN" CACHE STRING "" FORCE)
endif()

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

set(MESH_CRAFT_BUILD_TESTING "${BUILD_TESTING}" CACHE BOOL "Build MeshCraft tests")

set(BUILD_TESTING    OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(EASY_GL_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
set(EASY_GL_BUILD_EXAMPLES  OFF CACHE BOOL "" FORCE)
set(EASYGL_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(EASYGL_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)

# CNA — XNA-like runtime (window, input, audio, graphics)
add_subdirectory(../cna CNA_dep)

set(BUILD_TESTING "${MESH_CRAFT_BUILD_TESTING}" CACHE BOOL "" FORCE)

# Mc3 — MC3 format library (pure C++, no graphics dependency)
add_subdirectory(mc3)

# ImGui — immediate-mode UI (SDL3 + OpenGL ES 3 backends)
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)

# SDL3 headers: use prebuilt path from CNA's SDL3_DIR cache variable
# SDL3_DIR is .../lib/cmake/SDL3, so include dir is 3 levels up + /include
if(DEFINED SDL3_DIR)
    get_filename_component(_sdl3_inc "${SDL3_DIR}/../../../include" ABSOLUTE)
    target_include_directories(imgui PUBLIC "${_sdl3_inc}")
elseif(TARGET SDL3::SDL3)
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()
target_link_libraries(imgui PUBLIC GLESv2)

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------

file(GLOB_RECURSE SOURCES
        "src/MeshCraft/*.cpp"
        "src/MeshCraft/Editor/*.cpp"
        "src/MeshCraft/Editor/Tools/*.cpp"
        "src/MeshCraft/Renderer/*.cpp"
        "src/MeshCraft/Scene/*.cpp"
        "src/MeshCraft/Ui/*.cpp"
)

# -----------------------------------------------------------------------------
# Executable
# -----------------------------------------------------------------------------

if(ANDROID)
    set(_target main)
    add_library(${_target} SHARED ${SOURCES})
else()
    set(_target MeshCraft)
    add_executable(${_target} ${SOURCES})
endif()

if(EMSCRIPTEN)
    set_target_properties(${_target} PROPERTIES SUFFIX ".html")
endif()

target_include_directories(${_target}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -----------------------------------------------------------------------------
# Link
# -----------------------------------------------------------------------------
#
# CNA provides: windowing, input, audio, graphics (XNA-style API).
# SHARP_RUNTIME provides: math (Matrix, Vector3, …) used by the CNA backend.
# Mc3 provides: MC3 format data structures (pure C++).
#
# CNA and the graphics backend may have circular symbol references on Linux,
# so wrap them in a linker group (GNU ld / Clang ld on non-Windows).
# -----------------------------------------------------------------------------

if(MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "SDL_RENDERER")
    set(_cna_backend_target cna_backend_graphics_sdl_renderer)
elseif(MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "EASYGL")
    set(_cna_backend_target cna_backend_graphics_easygl)
elseif(MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "BGFX")
    set(_cna_backend_target cna_backend_graphics_bgfx)
elseif(MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "VULKAN")
    set(_cna_backend_target cna_backend_graphics_vulkan)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32 AND NOT EMSCRIPTEN AND NOT ANDROID)
    target_link_libraries(${_target} PRIVATE
        -Wl,--start-group CNA ${_cna_backend_target} -Wl,--end-group
        SHARP_RUNTIME
        Mc3
        imgui)
else()
    target_link_libraries(${_target} PRIVATE CNA ${_cna_backend_target} SHARP_RUNTIME Mc3 imgui)
endif()

# -----------------------------------------------------------------------------
# Windows / MinGW runtime
# -----------------------------------------------------------------------------

if(WIN32)
    cna_copy_sdl_runtime(${_target})
endif()

if(MINGW)
    target_link_options(${_target} PRIVATE -static-libgcc -static-libstdc++)
    cna_copy_mingw_runtime(${_target})
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------

if(MESH_CRAFT_BUILD_TESTING AND UNIX AND NOT ANDROID AND NOT EMSCRIPTEN)
    add_test(
        NAME smoke_test
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh
                $<TARGET_FILE:MeshCraft>
                ${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(smoke_test PROPERTIES TIMEOUT 30)
endif()
