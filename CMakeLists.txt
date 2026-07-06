cmake_minimum_required(VERSION 3.21)
project(MeshCraft VERSION 0.1.0 LANGUAGES C CXX)

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

if(ANDROID)
    set(MESH_CRAFT_GRAPHICS_BACKEND_UPPER "SDL_RENDERER")
elseif(EMSCRIPTEN)
    set(MESH_CRAFT_GRAPHICS_BACKEND_UPPER "EASYGL")
endif()

if(EMSCRIPTEN)
    add_compile_options(-fwasm-exceptions)
    add_link_options(-fwasm-exceptions)
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

# Mcb — MCB binary format library
add_subdirectory(mcb)

# mc3tomcb — CLI converter between MC3 XML and MCB binary
add_subdirectory(mc3tomcb)

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
if(NOT EMSCRIPTEN)
    target_link_libraries(imgui PUBLIC GLESv2)
endif()

# Manifold — CSG boolean mesh evaluation. Pinned at v3.0.0 (STAB-0222); no
# minimum-version requirement beyond this exact pin has been established —
# MeshCraft's CsgEvaluator.cpp only uses the stable, long-standing
# Manifold/MeshGL boolean-op API (+, -, ^, Transform), so any 3.x release
# is expected to work, but only v3.0.0 is actually built/tested here.
#
# All the FORCE-disabled options below are non-essential features this
# project doesn't use, kept off to minimize build complexity/time rather
# than because of a discovered technical blocker (no historical
# investigation found a specific reason to enable any of them):
#   MANIFOLD_EXPORT/TEST/FUZZ: Manifold's own export CLI, test suite, and
#     fuzzers -- not needed since MeshCraft only links the library.
#   MANIFOLD_PAR (STAB-0232): Manifold's internal (TBB/PSTL) parallelism
#     for its own mesh-processing algorithms. Left off; not yet profiled
#     against this project's typical scene sizes to see whether it would
#     help — a real perf investigation, not done, would need before/after
#     timing on a large CSG-heavy scene (see STAB-0233's perf test).
#   MANIFOLD_CROSS_SECTION (STAB-0235): Manifold's 2D cross-section/
#     polygon-boolean module (its `CrossSection` class) — unrelated to
#     MC3's own `Mc3CrossSection` (extrude cross-sections, a completely
#     separate, pre-existing MeshCraft feature). Left off since nothing in
#     mc3togltf calls into Manifold's CrossSection API.
set(MANIFOLD_PAR           OFF CACHE BOOL "" FORCE)
set(MANIFOLD_EXPORT        OFF CACHE BOOL "" FORCE)
set(MANIFOLD_TEST          OFF CACHE BOOL "" FORCE)
set(MANIFOLD_FUZZ          OFF CACHE BOOL "" FORCE)
set(MANIFOLD_CROSS_SECTION OFF CACHE BOOL "" FORCE)
FetchContent_Declare(manifold
    GIT_REPOSITORY https://github.com/elalish/manifold.git
    GIT_TAG        v3.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(manifold)

# tinyobjloader — OBJ mesh loading for viewport preview
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(tinyobjloader)

# mc3togltf — MC3 to glTF/GLB export tool (uses Mc3 + tinyobjloader already fetched above)
add_subdirectory(mc3togltf)

# SQLite3 — model registry persistence (desktop builds only). Optional,
# not REQUIRED: ModelRegistry.cpp's MESHCRAFT_HAS_SQLITE3 guard already
# provides safe no-op stubs for its absence (STAB-0008/0366/0552/0563),
# so a REQUIRED find_package() here would contradict that and needlessly
# block any environment/toolchain without a SQLite3 dev package for its
# target (e.g. a fresh MinGW cross-compile sysroot) — same pattern as the
# OpenSSL check right below.
if(NOT EMSCRIPTEN AND NOT ANDROID)
    find_package(SQLite3)
    if(SQLite3_FOUND)
        message(STATUS "SQLite3 found: ${SQLite3_VERSION}")
    else()
        message(STATUS "SQLite3 not found — ModelRegistry will be disabled at runtime")
    endif()
endif()

# cpp-httplib + OpenSSL — AI Assistant (desktop builds only)
if(NOT EMSCRIPTEN AND NOT ANDROID)
    set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.18.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(httplib)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        message(STATUS "OpenSSL found: ${OPENSSL_VERSION} — AI HTTPS enabled")
    else()
        message(STATUS "OpenSSL not found — AI Assistant will be disabled at runtime")
    endif()
endif()

# LibXml2 — XSD validation of AI-generated XML (STAB-0391, desktop builds only)
if(NOT EMSCRIPTEN AND NOT ANDROID)
    find_package(LibXml2)
    if(LIBXML2_FOUND)
        message(STATUS "LibXml2 found: ${LIBXML2_VERSION_STRING} — AI response XSD validation enabled")
    else()
        message(STATUS "LibXml2 not found — AI response XSD validation disabled (falls back to structural parse only)")
    endif()
endif()

# Embed mc3.xsd content into a generated header so runtime XSD validation
# never depends on the working directory or install layout.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mc3/mc3.xsd" MC3_XSD_CONTENT)
set(MESHCRAFT_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Mc3XsdEmbed.hpp.in"
    "${MESHCRAFT_GENERATED_INCLUDE_DIR}/MeshCraft/Mc3XsdEmbed.hpp"
    @ONLY
)

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
        ${tinyobjloader_SOURCE_DIR}
)

target_compile_definitions(${_target} PRIVATE MESHCRAFT_VERSION="${PROJECT_VERSION}")

# Enable SQLite3-backed ModelRegistry on desktop builds
if(SQLite3_FOUND)
    target_compile_definitions(${_target} PRIVATE MESHCRAFT_HAS_SQLITE3)
    target_include_directories(${_target} PRIVATE ${SQLite3_INCLUDE_DIRS})
endif()

# Enable AI Assistant when cpp-httplib + OpenSSL are available
if(httplib_POPULATED AND OpenSSL_FOUND)
    target_compile_definitions(${_target} PRIVATE MESHCRAFT_HAS_AI)
    target_link_libraries(${_target} PRIVATE httplib::httplib)
endif()

# Enable AI-response XSD validation when LibXml2 is available (STAB-0391)
target_include_directories(${_target} PRIVATE ${MESHCRAFT_GENERATED_INCLUDE_DIR})
if(LIBXML2_FOUND)
    target_compile_definitions(${_target} PRIVATE MESHCRAFT_HAS_LIBXML2)
    target_include_directories(${_target} PRIVATE ${LIBXML2_INCLUDE_DIR})
    target_link_libraries(${_target} PRIVATE ${LIBXML2_LIBRARIES})
endif()

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

if(EMSCRIPTEN)
    # SDL3 imported targets are directory-scoped; re-declare them in this scope.
    find_package(SDL3 REQUIRED CONFIG)
    target_link_libraries(${_target} PRIVATE
        CNA ${_cna_backend_target} SHARP_RUNTIME Mc3 Mcb mc3togltf_lib imgui SDL3::SDL3-static)
    target_link_options(${_target} PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=1048576
        -sINITIAL_MEMORY=134217728
        -sFORCE_FILESYSTEM=1
        "-sEXPORTED_RUNTIME_METHODS=['FS','ccall','cwrap']"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        -lidbfs.js
        "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/cmake/web/pre.js"
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/test@/test"
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32 AND NOT ANDROID)
    target_link_libraries(${_target} PRIVATE
        -Wl,--start-group CNA ${_cna_backend_target} -Wl,--end-group
        SHARP_RUNTIME
        Mc3
        Mcb
        mc3togltf_lib
        imgui
        manifold
        tinyobjloader)
    if(SQLite3_FOUND)
        target_link_libraries(${_target} PRIVATE ${SQLite3_LIBRARIES})
    endif()
else()
    target_link_libraries(${_target} PRIVATE CNA ${_cna_backend_target} SHARP_RUNTIME Mc3 Mcb mc3togltf_lib imgui manifold tinyobjloader)
    if(SQLite3_FOUND)
        target_link_libraries(${_target} PRIVATE ${SQLite3_LIBRARIES})
    endif()
endif()

# -----------------------------------------------------------------------------
# Windows / MinGW runtime
# -----------------------------------------------------------------------------

if(WIN32)
    cna_copy_sdl_runtime(${_target})

    # cna_copy_sdl_runtime() (cna/cmake/ThirdPartySDL.cmake) only copies DLLs
    # for SDL3::SDL3/SDL3_image::SDL3_image/SDL3_mixer::SDL3_mixer if those
    # targets are visible in the calling scope — but they're IMPORTED targets
    # created by find_package(SDL3 CONFIG) while processing CNA's own
    # add_subdirectory(), so they're invisible here in MeshCraft's parent
    # scope (confirmed via --trace-expand: all three if(TARGET ...) checks
    # evaluate false, so no copy commands are ever added — STAB-0566). Copy
    # them directly via the CACHE variable CNA sets for its prebuilt SDL
    # install root, which *is* visible everywhere.
    foreach(_sdl_dll IN ITEMS SDL3.dll SDL3_image.dll SDL3_mixer.dll)
        set(_sdl_dll_path "${CNA_SDL_PREBUILT_ROOT}/install/bin/${_sdl_dll}")
        if(EXISTS "${_sdl_dll_path}")
            add_custom_command(TARGET ${_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_sdl_dll_path}" "$<TARGET_FILE_DIR:${_target}>"
                COMMENT "Copying ${_sdl_dll} next to ${_target}"
                VERBATIM)
        endif()
    endforeach()
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
    set_tests_properties(smoke_test PROPERTIES TIMEOUT 30 LABELS "render")

    # STAB-0497: same smoke-test mechanism, but against a scene exercising
    # every Mc3::ObjectType (all_objects.mc3.xml) to confirm the live
    # SceneRenderer handles every type without crashing.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/all_objects.mc3.xml")
        add_test(
            NAME smoke_test_all_objects
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/all_objects.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(smoke_test_all_objects PROPERTIES TIMEOUT 30 LABELS "render")
    endif()

    # STAB-0498: same smoke-test mechanism, but against a scene with zero
    # objects, to confirm the parser/renderer handle a genuinely empty
    # scene without crashing.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/empty_scene.mc3.xml")
        add_test(
            NAME smoke_test_empty_scene
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/empty_scene.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(smoke_test_empty_scene PROPERTIES TIMEOUT 30 LABELS "render")
    endif()

    # STAB-0499: a <mesh> with a nonexistent src must render a placeholder
    # (not crash) and print a warning (not silently swallow the failure).
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/missing_mesh.mc3.xml")
        add_test(
            NAME missing_mesh_test
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/missing_mesh_test.sh
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/missing_mesh.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(missing_mesh_test PROPERTIES TIMEOUT 30 LABELS "render")
    endif()

    # STAB-0500: same smoke-test mechanism, but against an object whose
    # material id isn't declared anywhere in the document — confirms the
    # renderer falls back to a default color instead of crashing.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/missing_material.mc3.xml")
        add_test(
            NAME smoke_test_missing_material
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/missing_material.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(smoke_test_missing_material PROPERTIES TIMEOUT 30 LABELS "render")
    endif()

    # STAB-0507: default_camera points to an orthographic Mc3Camera, exercising
    # the CreateOrthographic "look through camera" branch (MeshCraftApplication.cpp)
    # — the same underlying projection-matrix mechanism the editor's own
    # Ortho/Persp UI toggle drives via EditorCamera::projectionMatrix().
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/orthographic_camera.mc3.xml")
        add_test(
            NAME smoke_test_orthographic_camera
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/orthographic_camera.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(smoke_test_orthographic_camera PROPERTIES TIMEOUT 30 LABELS "render")
    endif()

    find_program(PYTHON3_EXEC python3)
    if(PYTHON3_EXEC)
        execute_process(
            COMMAND "${PYTHON3_EXEC}" -c "import lxml"
            RESULT_VARIABLE _lxml_check_result
            ERROR_QUIET
        )
        if(NOT _lxml_check_result EQUAL 0)
            message(WARNING
                "xsd_validation test skipped: Python module 'lxml' not found. "
                "Install it with 'pip install lxml' or 'apt install python3-lxml' "
                "to enable mc3.xsd validation of test/*.mc3.xml.")
        else()
            file(GLOB _mc3_xml_files "${CMAKE_CURRENT_SOURCE_DIR}/test/*.mc3.xml")
            add_test(
                NAME xsd_validation
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/validate_xsd.py"
                        "${CMAKE_CURRENT_SOURCE_DIR}/mc3/mc3.xsd"
                        ${_mc3_xml_files}
            )
            set_tests_properties(xsd_validation PROPERTIES TIMEOUT 30 LABELS "format")
        endif()

        # STAB-0511: fog is a document-level <environment><fog> setting, applied
        # unconditionally (unlike the bloom/SSAO/wireframe UI toggles) — verify
        # a fully-fogged distant object actually renders fog-colored, not its
        # authored base color, via real pixel sampling.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/fog_linear.mc3.xml")
            add_test(
                NAME fog_linear_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/fog_linear_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/fog_linear.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(fog_linear_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0512: drawLightGizmos() is called unconditionally in the main
        # draw loop (not gated by selection/UI toggle like the translate/
        # rotate/bbox gizmos) — verify the point-light sphere+rays gizmo
        # actually renders via real pixel sampling.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/point_light_gizmo.mc3.xml")
            add_test(
                NAME point_light_gizmo_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/point_light_gizmo_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/point_light_gizmo.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(point_light_gizmo_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0513: same unconditional draw path as STAB-0512 — verify the
        # spot-light sphere+cone gizmo actually renders via real pixel
        # sampling.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/spot_light_gizmo.mc3.xml")
            add_test(
                NAME spot_light_gizmo_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/spot_light_gizmo_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/spot_light_gizmo.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(spot_light_gizmo_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0517: confirms lookThroughCamera_ actually overrides view/proj
        # from the Mc3Camera's own position/target/fov in --screenshot mode,
        # not silently falling back to the default editor camera.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/look_through_camera.mc3.xml")
            add_test(
                NAME look_through_camera_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/look_through_camera_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/look_through_camera.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(look_through_camera_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0522: confirms the content-hash CSG cache (K1) actually
        # prevents re-evaluating a static CSG object across the --screenshot
        # warm-up loop's ~120 frames.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_cache.mc3.xml")
            add_test(
                NAME csg_cache_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_cache_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_cache.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(csg_cache_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0523: confirms background_texture renders unconditionally,
        # stretched to fill the viewport, behind (not covering) 3D content.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/background_texture.mc3.xml")
            add_test(
                NAME background_texture_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/background_texture_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/background_texture.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(background_texture_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0524: confirms the equirectangular skybox shader actually
        # renders a document-driven skybox_texture.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/skybox_texture.mc3.xml")
            add_test(
                NAME skybox_texture_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/skybox_texture_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/skybox_texture.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(skybox_texture_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0525: confirms the LOD tier (SceneRenderer.cpp:633-639, G8)
        # actually changes with camera distance, not just reports a constant.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/lod_near.mc3.xml" AND
           EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/lod_far.mc3.xml")
            add_test(
                NAME lod_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/lod_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/lod_near.mc3.xml"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/lod_far.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(lod_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0526/0527/0528/0529: exercises the editor's --export CLI flag
        # (runGltfExport()'s non-interactive codepath) for GLB/glTF export,
        # the invalid-extension error path, and the exporter stats message.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml")
            add_test(
                NAME editor_export_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/editor_export_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(editor_export_test PROPERTIES TIMEOUT 60 LABELS "export")
        endif()
    endif()

    # mc3togltf_gltf test is registered by mc3togltf/CMakeLists.txt via $<TARGET_FILE:mc3togltf>

    # ModelRegistry unit tests (requires SQLite3)
    if(SQLite3_FOUND)
        add_executable(mc3_registry_test
            ${CMAKE_CURRENT_SOURCE_DIR}/mc3/test/mc3_registry_test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/ModelRegistry.cpp
        )
        target_include_directories(mc3_registry_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${SQLite3_INCLUDE_DIRS}
        )
        target_link_libraries(mc3_registry_test PRIVATE Mc3 ${SQLite3_LIBRARIES})
        target_compile_definitions(mc3_registry_test PRIVATE MESHCRAFT_HAS_SQLITE3)
        target_compile_features(mc3_registry_test PRIVATE cxx_std_23)
        add_test(NAME mc3_registry COMMAND mc3_registry_test)
        set_tests_properties(mc3_registry PROPERTIES TIMEOUT 15 LABELS "registry")
    endif()

    # AI Assistant unit + mock-HTTP-server tests (Gate 4, STAB-0371..0376).
    # AiResponseAlgorithms.hpp (extractXml/repairXml/parseXml/validate) has
    # no CNA/ImGui/network dependency, so this always builds; the mock HTTP
    # server round-trip additionally needs cpp-httplib + OpenSSL (same gate
    # as the main app's AI feature) to exercise AiAssistant::sendAsync() for
    # real, just against a local server instead of api.anthropic.com.
    if(httplib_POPULATED)
        add_executable(ai_test
            ${CMAKE_CURRENT_SOURCE_DIR}/mc3/test/ai_test.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/AiAssistant.cpp
        )
        target_include_directories(ai_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft
            ${MESHCRAFT_GENERATED_INCLUDE_DIR}
        )
        target_link_libraries(ai_test PRIVATE Mc3 httplib::httplib)
        if(OpenSSL_FOUND)
            target_compile_definitions(ai_test PRIVATE MESHCRAFT_HAS_AI)
        endif()
        if(LIBXML2_FOUND)
            target_compile_definitions(ai_test PRIVATE MESHCRAFT_HAS_LIBXML2)
            target_include_directories(ai_test PRIVATE ${LIBXML2_INCLUDE_DIR})
            target_link_libraries(ai_test PRIVATE ${LIBXML2_LIBRARIES})
        endif()
        target_compile_features(ai_test PRIVATE cxx_std_23)
        find_package(Threads REQUIRED)
        target_link_libraries(ai_test PRIVATE Threads::Threads)
        add_test(NAME mc3_ai COMMAND ai_test)
        set_tests_properties(mc3_ai PROPERTIES TIMEOUT 30 LABELS "ai")
    endif()
endif()
