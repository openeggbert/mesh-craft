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

# AUD-055: opt-in AddressSanitizer + UndefinedBehaviorSanitizer build for the
# first-party targets that parse untrusted input (Mc3's XML parser, Mcb's
# binary reader) or are otherwise part of this project's own surface --
# deliberately NOT applied to CNA/SHARP_RUNTIME/vendored deps (imgui,
# manifold, tinyobjloader, tinygltf, httplib): those are third-party/sibling
# code this project doesn't own (see CLAUDE.md -- no CNA changes without
# owner permission), so instrumenting them is out of scope here. This means
# a bug that only manifests across the instrumented/uninstrumented boundary
# may not be caught -- a real, accepted limitation of a first-party-only
# opt-in build, not full whole-program coverage.
#
#   cmake -S . -B build-asan -DMESHCRAFT_SANITIZE=ON
#   cmake --build build-asan && ctest --test-dir build-asan
option(MESHCRAFT_SANITIZE
       "Build first-party MeshCraft targets (Mc3/Mcb/mc3togltf/mc3tomcb/editor) with ASan+UBSan. Does not instrument CNA/SHARP_RUNTIME/vendored deps."
       OFF)
if(MESHCRAFT_SANITIZE)
    if(MSVC)
        message(WARNING "MESHCRAFT_SANITIZE is not wired up for MSVC; ignoring.")
        set(MESHCRAFT_SANITIZE OFF)
    else()
        set(MESHCRAFT_SANITIZE_COMPILE_OPTIONS -fsanitize=address,undefined -fno-omit-frame-pointer -g)
        # The link flag has to be applied at every EXECUTABLE's final link
        # step, not just the instrumented static libraries themselves --
        # otherwise a test executable that links an instrumented Mc3/Mcb/
        # mc3togltf_lib fails with "undefined reference to __asan_*" (the
        # object files reference the sanitizer runtime, but nothing tells
        # the final link to pull it in). add_link_options() here, before any
        # add_subdirectory() below, applies it to every target created
        # anywhere in this build -- including CNA/SHARP_RUNTIME/vendored
        # deps and the test executables in mc3/mcb/mc3togltf. Those
        # dependencies are not compiled WITH sanitizer instrumentation
        # (meshcraft_apply_sanitize() below is only called on first-party
        # targets), but linking the runtime into every final binary is
        # required for any of this to link at all, and is harmless for
        # non-instrumented object code.
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Applies the ASan/UBSan COMPILE-time instrumentation to `tgt` when
# MESHCRAFT_SANITIZE is ON; no-op otherwise. Called from this file and from
# the first-party subdirectories (mc3/mcb/mc3togltf/mc3tomcb) after each of
# their own target definitions -- MESHCRAFT_SANITIZE is a cache variable set
# here at the root, visible in every add_subdirectory() below it. Only
# instruments first-party code (see the option() help text above for why);
# the link-time runtime is wired in globally, above, since it must be.
function(meshcraft_apply_sanitize tgt)
    if(MESHCRAFT_SANITIZE AND TARGET ${tgt})
        target_compile_options(${tgt} PRIVATE ${MESHCRAFT_SANITIZE_COMPILE_OPTIONS})
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Graphics backend selection
# -----------------------------------------------------------------------------
#
# This variable selects the CNA *engine* backend. IMPORTANT: it is NOT the
# MeshCraft editor GUI backend. The editor's ImGui layer is hard-wired to
# OpenGL/GLES3 (ImGui_ImplOpenGL3 + ImGui_ImplSDL3_InitForOpenGL +
# SDL_GL_GetCurrentContext, see src/MeshCraft/MeshCraftApplication.cpp), so the
# GUI editor only renders under a GL context — i.e. EASYGL (desktop GL) or its
# Emscripten WebGL2 variant. SDL_RENDERER / BGFX / VULKAN configure CNA's engine
# but leave the editor UI unable to render (no matching ImGui backend, no GL
# context). Use them only for CNA-level experimentation or the CNA-free CLI
# tools (mc3togltf / mc3tomcb), which don't depend on the backend at all.
#
# Override with:
#   cmake -S . -B build -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL   # editor GUI works
#   cmake -S . -B build -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER  # CNA engine only
#
# Accepted (CNA engine): SDL_RENDERER, EASYGL, BGFX, VULKAN.
# Functional for the MeshCraft editor GUI: EASYGL only.
# -----------------------------------------------------------------------------

set(MESH_CRAFT_GRAPHICS_BACKEND "EASYGL" CACHE STRING
        "CNA engine backend. The editor GUI only renders on EASYGL.")
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

message(STATUS "MeshCraft graphics backend (CNA engine): ${MESH_CRAFT_GRAPHICS_BACKEND_UPPER}")

# Gate C truthfulness: the editor GUI only renders on EASYGL. Warn loudly for any
# other backend so a non-functional editor build is never produced silently. The
# CNA-free CLI tools are unaffected. Set MESH_CRAFT_SILENCE_BACKEND_WARNING=ON to
# suppress this when you are deliberately building CNA-only / a CLI-only target.
if(NOT MESH_CRAFT_GRAPHICS_BACKEND_UPPER STREQUAL "EASYGL"
        AND NOT MESH_CRAFT_SILENCE_BACKEND_WARNING)
    message(WARNING
        "MESH_CRAFT_GRAPHICS_BACKEND='${MESH_CRAFT_GRAPHICS_BACKEND_UPPER}' selects the "
        "CNA engine backend, but the MeshCraft *editor GUI* is hard-wired to OpenGL/GLES3 "
        "(ImGui_ImplOpenGL3). Under this backend the editor will build but its UI will NOT "
        "render (no GL context / no matching ImGui backend). Use EASYGL for a working editor; "
        "the CNA-free CLI tools (mc3togltf, mc3tomcb) work under any backend. "
        "Set -DMESH_CRAFT_SILENCE_BACKEND_WARNING=ON to silence this.")
endif()

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
# MeshCraft is a single-user desktop editor with no multiplayer/Xbox-Live-
# style networking — it never links CNA_GamerServices or CNA_Net (confirmed:
# neither appears in this file's target_link_libraries calls). CNA_ENABLE_NET
# defaults to ON in ../cna/CMakeLists.txt, unconditionally pulling both into
# the default `ninja`/`all` target even though nothing here needs them.
# Disabling it removes an unused, un-built-by-MeshCraft component from the
# default build graph — not a CNA source change, just not opting into an
# optional CNA feature this project doesn't use.
set(CNA_ENABLE_NET OFF CACHE BOOL "" FORCE)

# -----------------------------------------------------------------------------
# AUD-057: sibling-repo revision check (informational, configure-time)
#
# ../cna and ../sharp-runtime are separate git repositories consumed by
# relative path (not a submodule or pinned dependency) -- whatever happens to
# be checked out there is what gets built. That is non-reproducible and,
# per README.md, has previously broken a from-scratch rebuild without
# warning. A hard version PIN with a FATAL_ERROR on mismatch is deliberately
# NOT used here: CNA is developed by a separate process (see CLAUDE.md --
# "No CNA changes without owner permission. A separate Claude Code instance
# handles CNA.") and legitimately moves ahead of whatever revision this
# project last verified against; failing configure on every such move would
# block that work. Instead this prints a non-fatal heads-up so a drifted
# sibling revision is the first, not the last, suspect when something breaks.
# Update the two *_VERIFIED_SHA values below after re-running `ctest` against
# a newer sibling checkout and confirming it still passes.
# -----------------------------------------------------------------------------
set(MESHCRAFT_CNA_VERIFIED_SHA "d0c21ee613ebd8f8b0055f7868103dd5c5c30fc7")
set(MESHCRAFT_SHARP_RUNTIME_VERIFIED_SHA "5cdaafb2bace46dce5393da21dad9ff9f8ad3c58")

function(meshcraft_check_sibling_revision name path expected_sha)
    if(EXISTS "${path}/.git")
        execute_process(
            COMMAND git -C "${path}" rev-parse HEAD
            OUTPUT_VARIABLE _actual_sha
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _git_rc
            ERROR_QUIET
        )
        if(_git_rc EQUAL 0 AND NOT _actual_sha STREQUAL expected_sha)
            message(WARNING
                "AUD-057: ${name} at '${path}' is checked out at ${_actual_sha}, "
                "not the revision (${expected_sha}) MeshCraft's own test "
                "suite was last verified against. Not necessarily broken -- "
                "${name} is developed independently -- but if the build or "
                "tests fail, this drift is a likely first suspect. Re-verify "
                "with `ctest` and update MESHCRAFT_${name}_VERIFIED_SHA in "
                "CMakeLists.txt once confirmed working.")
        endif()
    endif()
endfunction()

meshcraft_check_sibling_revision(CNA "${CMAKE_CURRENT_SOURCE_DIR}/../cna" "${MESHCRAFT_CNA_VERIFIED_SHA}")
meshcraft_check_sibling_revision(SHARP_RUNTIME "${CMAKE_CURRENT_SOURCE_DIR}/../sharp-runtime" "${MESHCRAFT_SHARP_RUNTIME_VERIFIED_SHA}")

# SYS-W14-15: enables CNA::Devices::FileDialog (native open/save dialogs) for
# the editor's texture/mesh path fields. CNA_DEVICES is an existing CNA-side
# CMake option (off by default, "device/sensor extensions beyond XNA 4.0");
# forcing it on here is a mesh-craft-side consumer choice, not a CNA edit.
set(CNA_DEVICES ON CACHE BOOL "" FORCE)

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

# Lua 5.4 + sol2 — SYS-W14-18 (2026-07-20): Mc3Object::scriptId/doc.scripts
# ("lua") round-tripped/were editable but nothing ever executed a script.
# Mirrors the sibling ../mesh-world repo's own already-working, sandboxed
# setup exactly (library choice, versions, sandbox discipline) rather than
# inventing a different one -- see its Mc3ScriptRunner.cpp/LuaRuntime.cpp.
# mesh-world deliberately kept its Lua/sol2 dependency OUT of the shared
# mc3/ library (also consumed by CNA/mc3togltf/mc3tomcb) to avoid forcing
# it on every mc3/ consumer; that constraint doesn't apply here since this
# is the MeshCraft EDITOR target specifically (which already vendors much
# heavier dependencies like Manifold/ImGui), so it's added directly to it,
# not to mc3/. Pure portable C/C++, unconditional like ImGui/Manifold
# above -- not gated behind NOT EMSCRIPTEN/NOT ANDROID like SQLite3/
# cpp-httplib below (those gate on external system libraries; Lua is
# self-contained and commonly compiled to WASM/Android already).
FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.7
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(lua)
file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
# Exclude interpreter/compiler mains (lua.c/luac.c both define main()),
# matching mesh-world's own exclusion list. ALSO exclude onelua.c and
# ltests.c -- mesh-world fetched the lua.org release TARBALL, which
# doesn't include either; this fetches the git mirror instead (matching
# this repo's own GIT_REPOSITORY/GIT_TAG FetchContent convention), whose
# checkout does include them. onelua.c #includes every other .c file
# verbatim into one translation unit (its own header comment: "Lua core,
# libraries, and interpreter in a single file") -- compiling it ALONGSIDE
# the individual .c files below would duplicate-define every Lua symbol,
# a link-time failure once something actually pulls both object files in
# (confirmed: onelua.c.o built "successfully" as an object file on its
# own, since a static library's ar step doesn't detect the conflict --
# only linking a consumer that references these symbols would). ltests.c
# is Lua's own internal test/diagnostics module, not needed for embedding.
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/luac\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/onelua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/ltests\\.c$")
add_library(lua54 STATIC ${LUA_SOURCES})
target_include_directories(lua54 PUBLIC "${lua_SOURCE_DIR}")
if(NOT MSVC)
    target_compile_options(lua54 PRIVATE -w)
else()
    target_compile_options(lua54 PRIVATE /w)
endif()

FetchContent_Declare(sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        v3.3.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(sol2)
# sol2's own CMakeLists.txt creates the sol2::sol2 INTERFACE target.

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

# AUD-054: -Wall -Wextra was previously enabled on only 2 of the first-party
# targets (Mc3, mc3togltf_lib) -- the editor itself (32 files / 18.5k LOC,
# the largest first-party surface) compiled with no warning flags at all.
# PRIVATE so this only applies to MeshCraft's own translation units, not
# anything consuming this target.
target_compile_options(${_target} PRIVATE -Wall -Wextra)
meshcraft_apply_sanitize(${_target})

# AUD-039b: the configure-time warning above (Gate C truthfulness) does not
# stop a non-functional editor binary from being built and run -- it only
# prints a message a headless/CI configure or a GUI CMake frontend can easily
# miss. Wrapping this whole target in a CMake conditional to skip building it
# entirely under non-EASYGL backends would require restructuring several
# hundred lines of existing, stabilized target configuration (sources, link
# libraries, tests all reference ${_target}) for high regression risk. A
# runtime check is the surgical alternative that still satisfies Gate C
# ("unsupported and rejected clearly"): embed the selected backend name, and
# main() (src/MeshCraft/main.cpp) refuses to proceed under a backend the
# editor cannot render on, before any window/GL initialization -- an
# unsupported configuration is REJECTED at startup with a clear message and a
# non-zero exit code, not silently launched as a broken window. The CLI tools
# (mc3togltf/mc3tomcb) do not link this target and are unaffected.
target_compile_definitions(${_target} PRIVATE
    MESH_CRAFT_GRAPHICS_BACKEND_STR="${MESH_CRAFT_GRAPHICS_BACKEND_UPPER}")

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
        CNA ${_cna_backend_target} SHARP_RUNTIME Mc3 Mcb mc3togltf_lib imgui SDL3::SDL3-static
        lua54 sol2::sol2)
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
        tinyobjloader
        lua54
        sol2::sol2)
    if(SQLite3_FOUND)
        target_link_libraries(${_target} PRIVATE ${SQLite3_LIBRARIES})
    endif()
else()
    target_link_libraries(${_target} PRIVATE CNA ${_cna_backend_target} SHARP_RUNTIME Mc3 Mcb mc3togltf_lib imgui manifold tinyobjloader lua54 sol2::sol2)
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
    # Pure unit test: ActiveTool enum <-> name mapping. Header-only, no CNA/GL
    # deps, so it always builds. Regression guard for the Measure-tool
    # out-of-bounds title read (activeToolName exhaustive switch).
    add_executable(active_tool_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/active_tool_test.cpp)
    target_include_directories(active_tool_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(active_tool_test PRIVATE cxx_std_23)
    add_test(NAME active_tool COMMAND active_tool_test)
    set_tests_properties(active_tool PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01: Editor::KeybindingManager, extracted from
    # MeshCraftApplication (H9). No test existed for this subsystem before
    # the extraction. Needs real Microsoft::Xna::Framework::Input::Keys/
    # KeyboardState (CNA), so unlike active_tool_test/object_type_name_test
    # above it links CNA + SHARP_RUNTIME (SDL3 comes in transitively via CNA
    # on this native, non-Emscripten path -- see the "Link" section below for
    # why SDL3 isn't named explicitly here) -- but not Mc3/Mcb/
    # mc3togltf_lib/imgui, which this subsystem has no dependency on.
    add_executable(keybinding_manager_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/keybinding_manager_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/KeybindingManager.cpp)
    target_include_directories(keybinding_manager_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(keybinding_manager_test PRIVATE
        CNA ${_cna_backend_target} SHARP_RUNTIME)
    target_compile_features(keybinding_manager_test PRIVATE cxx_std_23)
    add_test(NAME keybinding_manager COMMAND keybinding_manager_test)
    set_tests_properties(keybinding_manager PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01 Phase 2 (narrow): Editor::Preferences, extracted from
    # MeshCraftApplication (H7). No test existed for this subsystem before
    # the extraction. Needs only ImGui's core (no CNA/window/GL context),
    # matching undo_gesture_frame_test's minimal-dependency pattern above.
    add_executable(preferences_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/preferences_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/Preferences.cpp)
    target_include_directories(preferences_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(preferences_test PRIVATE imgui)
    target_compile_features(preferences_test PRIVATE cxx_std_23)
    add_test(NAME preferences COMMAND preferences_test)
    set_tests_properties(preferences PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W5-04: Editor::ObjectIndex, a lazily-rebuilt id/name -> object
    # cache over Mc3Document.objects extracted out of
    # MeshCraftApplication::flatFindById()/flatFindByName(). CNA-free (only
    # depends on Mc3Object/Mc3Document, which are header-only for this
    # class's purposes -- no factory methods are called), so unlike
    # keybinding_manager_test above this needs neither CNA nor Mc3 linked.
    add_executable(object_index_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/object_index_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/ObjectIndex.cpp)
    target_include_directories(object_index_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_compile_features(object_index_test PRIVATE cxx_std_23)
    add_test(NAME object_index COMMAND object_index_test)
    set_tests_properties(object_index PROPERTIES TIMEOUT 15 LABELS "unit")

    # F9 (2026-07-20 audit): registerTextureFromPathAlg() (EditorAlgorithms.hpp),
    # backing MeshCraftApplication::registerTextureFromPath(). CNA-free --
    # EditorAlgorithms.hpp only needs Mc3Document/Mc3Object (header-only),
    # matching object_index_test above.
    add_executable(texture_from_path_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/texture_from_path_test.cpp)
    target_include_directories(texture_from_path_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_compile_features(texture_from_path_test PRIVATE cxx_std_23)
    add_test(NAME texture_from_path COMMAND texture_from_path_test)
    set_tests_properties(texture_from_path PROPERTIES TIMEOUT 15 LABELS "unit")

    # F10 (2026-07-20 audit): clear*ReferencesAlg()/removeTriggerStepsReferencingAlg()
    # (EditorAlgorithms.hpp), backing the Tex/Defs/Mat/Scripts/Sounds/Music/
    # Embeds Delete buttons' reference-integrity cleanup. CNA-free, matching
    # object_index_test/texture_from_path_test above.
    add_executable(delete_reference_integrity_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/delete_reference_integrity_test.cpp)
    target_include_directories(delete_reference_integrity_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_compile_features(delete_reference_integrity_test PRIVATE cxx_std_23)
    add_test(NAME delete_reference_integrity COMMAND delete_reference_integrity_test)
    set_tests_properties(delete_reference_integrity PROPERTIES TIMEOUT 15 LABELS "unit")

    # F19 (2026-07-20 audit): runObjExport()'s intermediate .glb temp path
    # used to be derived from `this`'s pointer value (deterministic --
    # collision risk across concurrent instances) and was only cleaned up
    # on the success path (leaked on exportDocument()/LoadBinaryFromFile()
    # throwing). Mirrors the fixed control-flow shape with real
    # uniqueTempPath()/std::filesystem calls -- CNA-free, header-only
    # (MeshCraft/TempFile.hpp), matching object_index_test's precedent.
    add_executable(obj_export_temp_cleanup_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/obj_export_temp_cleanup_test.cpp)
    target_include_directories(obj_export_temp_cleanup_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(obj_export_temp_cleanup_test PRIVATE cxx_std_23)
    add_test(NAME obj_export_temp_cleanup COMMAND obj_export_temp_cleanup_test)
    set_tests_properties(obj_export_temp_cleanup PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01 Phase 3: Editor::MacroRecorder, extracted from
    # MeshCraftApplication (H12). No test existed for this subsystem before
    # the extraction. CNA-free -- executeStep()'s cross-domain calls
    # (addPrimitive/deleteSelected/document_/selection_/etc.) are resolved
    # through a Context of std::function callbacks (a PropertiesContext-
    # style DI struct) rather than linking MeshCraftApplication itself, so
    # this only needs Mc3Object.hpp's ObjectType enum (header-only) plus
    # EditorAlgorithms.hpp's save/load helpers -- neither needs Mc3/CNA
    # linked, matching object_index_test above.
    add_executable(macro_recorder_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/macro_recorder_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/MacroRecorder.cpp)
    target_include_directories(macro_recorder_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_compile_features(macro_recorder_test PRIVATE cxx_std_23)
    add_test(NAME macro_recorder COMMAND macro_recorder_test)
    set_tests_properties(macro_recorder PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01 Phase 4: Editor::UndoManager, extracted from
    # MeshCraftApplication (the undo/redo stack mechanism, SYS-W9-03's
    # selection-restore stacks included). No test existed for this
    # subsystem before the extraction (undo_gesture_frame_test.cpp covers
    # the same invariants against hand-rolled stand-ins, since
    # MeshCraftApplication's real pushUndo()/performUndo()/performRedo()
    # aren't headlessly callable -- this exercises the real class
    # directly). CNA-free -- unlike MacroRecorder, this needs no callback
    # Context at all: every side effect beyond stack bookkeeping
    # (deepCopyDoc(), objectIndex_ invalidation, selection restore) stays
    # the caller's responsibility, so this only needs Mc3Document.hpp
    # (header-only), matching object_index_test above.
    add_executable(undo_manager_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/undo_manager_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/UndoManager.cpp)
    target_include_directories(undo_manager_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_compile_features(undo_manager_test PRIVATE cxx_std_23)
    add_test(NAME undo_manager COMMAND undo_manager_test)
    set_tests_properties(undo_manager PROPERTIES TIMEOUT 15 LABELS "unit")

    # F7 (2026-07-20 audit): Model Registry Insert's undo ordering.
    # drawRegistryPanel()'s Insert handler (MeshCraftApplication_UiRegistry.cpp)
    # is not headlessly callable (full ImGui panel tied into the
    # application), so this exercises the real Editor::UndoManager against a
    # hand-rolled stand-in matching the handler's exact fixed control-flow
    # shape (pushUndo() before insertIntoScene(), popUndoWithoutApplying()
    # on failure). CNA-free, same dependency footprint as undo_manager_test.
    add_executable(registry_insert_undo_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/registry_insert_undo_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/UndoManager.cpp)
    target_include_directories(registry_insert_undo_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_compile_features(registry_insert_undo_test PRIVATE cxx_std_23)
    add_test(NAME registry_insert_undo COMMAND registry_insert_undo_test)
    set_tests_properties(registry_insert_undo PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01 Phase 6: Editor::WalkController, extracted from
    # MeshCraftApplication (H15, first-person walk mode). No test existed
    # for this subsystem before the extraction. Needs real
    # Microsoft::Xna::Framework::Input::Keys/KeyboardState/Matrix/Vector3
    # (CNA), so like keybinding_manager_test above it links CNA +
    # SHARP_RUNTIME -- but not Mc3/Mcb/mc3togltf_lib/imgui, which this
    # subsystem has no dependency on.
    add_executable(walk_controller_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/walk_controller_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/WalkController.cpp)
    target_include_directories(walk_controller_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(walk_controller_test PRIVATE
        CNA ${_cna_backend_target} SHARP_RUNTIME)
    target_compile_features(walk_controller_test PRIVATE cxx_std_23)
    add_test(NAME walk_controller COMMAND walk_controller_test)
    set_tests_properties(walk_controller PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01 Phase 7: Editor::AudioPreview, extracted from
    # MeshCraftApplication (STAB-0706, sound/music preview playback). No
    # test existed for this subsystem before the extraction. Needs real
    # Microsoft::Xna::Framework::Audio::SoundEffect/SoundEffectInstance
    # (CNA), so like keybinding_manager_test/walk_controller_test above it
    # links CNA + SHARP_RUNTIME.
    add_executable(audio_preview_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/audio_preview_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/AudioPreview.cpp)
    target_include_directories(audio_preview_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(audio_preview_test PRIVATE
        CNA ${_cna_backend_target} SHARP_RUNTIME)
    target_compile_features(audio_preview_test PRIVATE cxx_std_23)
    add_test(NAME audio_preview COMMAND audio_preview_test)
    set_tests_properties(audio_preview PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01 Phase 8: orbit-camera bookmark capture/restore was extracted
    # from MeshCraftApplication. It uses EditorCamera's CNA Vector3 type but
    # needs no window, scene document, or ImGui frame.
    add_executable(camera_bookmarks_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/camera_bookmarks_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/CameraBookmarks.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/EditorCamera.cpp)
    target_include_directories(camera_bookmarks_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_link_libraries(camera_bookmarks_test PRIVATE
        CNA ${_cna_backend_target} SHARP_RUNTIME)
    target_compile_features(camera_bookmarks_test PRIVATE cxx_std_23)
    add_test(NAME camera_bookmarks COMMAND camera_bookmarks_test)
    set_tests_properties(camera_bookmarks PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01 Phase 9: transform clipboard P/R/S capture and transfer is
    # pure MC3 state, independently of selection, undo, and ImGui wiring.
    add_executable(transform_clipboard_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/transform_clipboard_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/TransformClipboard.cpp)
    target_include_directories(transform_clipboard_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_compile_features(transform_clipboard_test PRIVATE cxx_std_23)
    add_test(NAME transform_clipboard COMMAND transform_clipboard_test)
    set_tests_properties(transform_clipboard PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W3-01 Phase 10: timed status-notification state/expiry has no
    # framework dependency and can be checked without a MeshCraft window.
    add_executable(status_notification_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/status_notification_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/StatusNotification.cpp)
    target_include_directories(status_notification_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(status_notification_test PRIVATE cxx_std_23)
    add_test(NAME status_notification COMMAND status_notification_test)
    set_tests_properties(status_notification PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W14-18 (2026-07-20): LuaScriptRunner -- executes Mc3Script
    # ("lua") source against a document. Unlike the CNA-coupled UI wiring
    # (Scripts tab / Triggers tab, tested via mirrors elsewhere), this
    # class itself is CNA-free, so this exercises it directly with real
    # Lua execution (sandbox, def:place()/place_at()/has_socket(),
    # scene:find() + property read/write, the instruction-budget guard
    # against an actual infinite loop).
    add_executable(lua_script_runner_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/lua_script_runner_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/LuaScriptRunner.cpp)
    target_include_directories(lua_script_runner_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_link_libraries(lua_script_runner_test PRIVATE lua54 sol2::sol2)
    target_compile_features(lua_script_runner_test PRIVATE cxx_std_23)
    add_test(NAME lua_script_runner COMMAND lua_script_runner_test)
    set_tests_properties(lua_script_runner PROPERTIES TIMEOUT 30 LABELS "unit")

    # Pure unit test: ObjectType <-> display-name round-trip. Header-only.
    # Regression guard for the Torus/Capsule/Disk/Grid/IcoSphere -> "Object"
    # mislabel and the lossy macro round-trip.
    add_executable(object_type_name_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/object_type_name_test.cpp)
    target_include_directories(object_type_name_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_link_libraries(object_type_name_test PRIVATE Mc3)
    target_compile_features(object_type_name_test PRIVATE cxx_std_23)
    add_test(NAME object_type_name COMMAND object_type_name_test)
    set_tests_properties(object_type_name PROPERTIES TIMEOUT 15 LABELS "unit")

    # AUD-039b (Gate C): pure decision logic for the editor's runtime
    # backend-support check. Header-only, no CNA/GL dependency.
    add_executable(graphics_backend_check_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/graphics_backend_check_test.cpp)
    target_include_directories(graphics_backend_check_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(graphics_backend_check_test PRIVATE cxx_std_23)
    add_test(NAME graphics_backend_check COMMAND graphics_backend_check_test)
    set_tests_properties(graphics_backend_check PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W7-02: differential geometry tests -- viewport renderer (via the
    # CNA-free tessellateUnit*Alg() mirror in
    # include/MeshCraft/Renderer/PrimitiveTessellationAlg.hpp) vs. the glTF
    # exporter's independent mc3togltf_lib::buildPrimitive(). No CNA/GL
    # dependency -- links Mc3 (transitively, via mc3togltf_lib) + mc3togltf_lib
    # directly, exactly like mc3togltf_earclip_test above.
    add_executable(differential_geometry_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/differential_geometry_test.cpp)
    target_include_directories(differential_geometry_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(differential_geometry_test PRIVATE mc3togltf_lib)
    target_compile_features(differential_geometry_test PRIVATE cxx_std_23)
    target_compile_options(differential_geometry_test PRIVATE -Wall -Wextra)
    add_test(NAME differential_geometry COMMAND differential_geometry_test)
    set_tests_properties(differential_geometry PROPERTIES TIMEOUT 30 LABELS "unit")

    # 2026-07-20 audit finding #2 (P1): segments in {0,1} made several
    # primitive tessellators -- in BOTH independent geometry generators
    # (mc3togltf::MeshBuilder.cpp and PrimitiveTessellationAlg.hpp) --
    # divide by an unclamped zero inside a "<= segments"-bounded loop that
    # still executes once, producing a NaN vertex position. No CNA/GL
    # dependency, same idiom as differential_geometry_test above.
    add_executable(primitive_zero_segments_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/primitive_zero_segments_test.cpp)
    target_include_directories(primitive_zero_segments_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(primitive_zero_segments_test PRIVATE mc3togltf_lib)
    target_compile_features(primitive_zero_segments_test PRIVATE cxx_std_23)
    target_compile_options(primitive_zero_segments_test PRIVATE -Wall -Wextra)
    add_test(NAME primitive_zero_segments COMMAND primitive_zero_segments_test)
    set_tests_properties(primitive_zero_segments PROPERTIES TIMEOUT 15 LABELS "unit")

    # AUD-073: SceneRenderer_Extrude.cpp's hollow/innerScale formula (both
    # drawExtrudeDynamic() and drawObjectEdges()) divided by cross-section
    # radius with no zero-guard. SceneRenderer is CNA-coupled and can't be
    # unit-tested directly, so this mirrors the exact two-line formula --
    # same idiom as differential_geometry_test above. No CNA/GL dependency,
    # links only Mc3 (for Mc3CrossSection/CrossSectionType).
    add_executable(extrude_hollow_zero_radius_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/extrude_hollow_zero_radius_test.cpp)
    target_link_libraries(extrude_hollow_zero_radius_test PRIVATE Mc3)
    target_compile_features(extrude_hollow_zero_radius_test PRIVATE cxx_std_23)
    target_compile_options(extrude_hollow_zero_radius_test PRIVATE -Wall -Wextra)
    add_test(NAME extrude_hollow_zero_radius COMMAND extrude_hollow_zero_radius_test)
    set_tests_properties(extrude_hollow_zero_radius PROPERTIES TIMEOUT 15 LABELS "unit")

    # 2026-07-20 audit F5: extrudeEdgeOverlayExceedsVertexBudgetAlg()
    # (PrimitiveTessellationAlg.hpp) is a pure, header-only function with no
    # CNA/Mc3 dependency -- calls it directly, not a mirror.
    add_executable(extrude_edge_overlay_budget_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/extrude_edge_overlay_budget_test.cpp)
    target_include_directories(extrude_edge_overlay_budget_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(extrude_edge_overlay_budget_test PRIVATE cxx_std_23)
    target_compile_options(extrude_edge_overlay_budget_test PRIVATE -Wall -Wextra)
    add_test(NAME extrude_edge_overlay_budget COMMAND extrude_edge_overlay_budget_test)
    set_tests_properties(extrude_edge_overlay_budget PROPERTIES TIMEOUT 15 LABELS "unit")

    # AUD-036b: frame-driven regression test for the undo-snapshot-timing fix.
    # Drives real ImGui frames through a full click-drag gesture on
    # DragFloat/DragFloat3/ColorEdit3 (the widget classes AUD-036 fixed) plus
    # SliderFloat as a control case, and asserts exactly one undo snapshot
    # fires per gesture, on/before the first mutation frame -- not just that
    # the fixed source pattern is present (undo_snapshot_lint_test.py already
    # checks that), but that it behaves correctly across real frames. Needs
    # only ImGui's core (no CNA/window/GL context; the imgui target link deps
    # are unused at runtime here).
    add_executable(undo_gesture_frame_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/undo_gesture_frame_test.cpp)
    target_link_libraries(undo_gesture_frame_test PRIVATE imgui)
    target_compile_features(undo_gesture_frame_test PRIVATE cxx_std_23)
    add_test(NAME undo_gesture_frame COMMAND undo_gesture_frame_test)
    set_tests_properties(undo_gesture_frame PROPERTIES TIMEOUT 15 LABELS "unit")

    # F20 (2026-07-20 audit): the "Env" left-panel tab's Fog Start/End
    # fields used to clamp each independently to [0,10000], letting Start
    # end up greater than End -- unlike Scene/PropertiesPanel.cpp's own
    # Fog editor, which was already cross-clamped. Drives the exact shared
    # DragFloat pattern both editors now use directly against a plain
    # Mc3Fog struct -- CNA-free (no MeshCraftApplication/PropertiesPanel
    # dependency), matching undo_gesture_frame_test's approach.
    add_executable(fog_start_end_cross_clamp_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/fog_start_end_cross_clamp_test.cpp)
    target_include_directories(fog_start_end_cross_clamp_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_link_libraries(fog_start_end_cross_clamp_test PRIVATE imgui)
    target_compile_features(fog_start_end_cross_clamp_test PRIVATE cxx_std_23)
    add_test(NAME fog_start_end_cross_clamp COMMAND fog_start_end_cross_clamp_test)
    set_tests_properties(fog_start_end_cross_clamp PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W14-19 (2026-07-20): the Triggers tab's new "Fire" action --
    # actually executes a trigger's steps (PlayAction/PlaySound/PlayMusic/
    # RunScript) instead of triggers being purely round-tripped data with
    # no way to run them. MeshCraftApplication_UiLeftPanel.cpp's
    # fireTrigger() is CNA-coupled (not headlessly instantiable), so this
    # mirrors its exact control-flow shape against plain Mc3Document data
    # and mock playback/audio trackers, matching this session's
    # established pattern (registry_insert_undo_test/obj_export_temp_cleanup_test).
    add_executable(trigger_fire_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/trigger_fire_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/LuaScriptRunner.cpp)
    target_include_directories(trigger_fire_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_link_libraries(trigger_fire_test PRIVATE lua54 sol2::sol2)
    target_compile_features(trigger_fire_test PRIVATE cxx_std_23)
    add_test(NAME trigger_fire COMMAND trigger_fire_test)
    set_tests_properties(trigger_fire PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W14-20 (2026-07-20): the States tab's new "Apply State" action --
    # actually writes a scene state's overrides onto the matching live
    # objects instead of states being purely round-tripped data with no
    # way to preview them. CNA-free mirror (MeshCraftApplication is not
    # headlessly instantiable), matching trigger_fire_test's own pattern.
    add_executable(scene_state_apply_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/scene_state_apply_test.cpp)
    target_include_directories(scene_state_apply_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_compile_features(scene_state_apply_test PRIVATE cxx_std_23)
    add_test(NAME scene_state_apply COMMAND scene_state_apply_test)
    set_tests_properties(scene_state_apply PROPERTIES TIMEOUT 15 LABELS "unit")

    # SYS-W14-21 (2026-07-20): the Imports tab's new "Resolve Imports" button
    # and the automatic post-load resolution -- resolveImports() wires the
    # already-tested Mc3ImportResolver (R101) into the editor. CNA-free
    # mirror of resolveImports()'s exact control flow (MeshCraftApplication
    # is not headlessly instantiable), against the real Mc3ImportResolver and
    # real Mc3Document -- matching trigger_fire_test/scene_state_apply_test's
    # established pattern. Links Mc3 for Mc3ImportResolver/Mc3Document's
    # compiled methods (saveToLibraryFile, computeLibraryContentHash, ...).
    add_executable(resolve_imports_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/resolve_imports_test.cpp)
    target_link_libraries(resolve_imports_test PRIVATE Mc3)
    target_compile_features(resolve_imports_test PRIVATE cxx_std_23)
    add_test(NAME resolve_imports COMMAND resolve_imports_test)
    set_tests_properties(resolve_imports PROPERTIES TIMEOUT 15 LABELS "unit")

    # 2026-07-20 audit F2 (P0): SceneHierarchyPanel.cpp's drawHierarchy()
    # range-for-iterates over document_.objects/children by reference;
    # dragging an earlier sibling onto a later one in the same list used to
    # call doReparent() (which erases from that same live vector)
    # synchronously mid-loop -- real iterator-invalidation UB, reachable via
    # ordinary drag-and-drop. Drives a REAL multi-frame ImGui drag gesture
    # (same technique as undo_gesture_frame_test.cpp's driveClickDragGesture)
    # between two actual tree rows rendered by the real SceneHierarchyPanel
    # class, not a mock -- CNA-free (SceneHierarchyPanel.cpp only depends on
    # imgui + EditorAlgorithms.hpp (header-only) + Mc3Object/SelectionManager),
    # so this links Mc3 + imgui only, no CNA/window/GL context.
    add_executable(scene_hierarchy_panel_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/scene_hierarchy_panel_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Scene/SceneHierarchyPanel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft/Editor/SelectionManager.cpp)
    target_include_directories(scene_hierarchy_panel_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/mc3/include)
    target_link_libraries(scene_hierarchy_panel_test PRIVATE Mc3 imgui)
    target_compile_features(scene_hierarchy_panel_test PRIVATE cxx_std_23)
    add_test(NAME scene_hierarchy_panel COMMAND scene_hierarchy_panel_test)
    set_tests_properties(scene_hierarchy_panel PROPERTIES TIMEOUT 15 LABELS "unit")

    # Source-lint guard: the dead undo-snapshot pattern (undo nested inside a
    # Drag*/ColorEdit* changed-block, which never fires) must not reappear.
    find_program(PYTHON3_FOR_LINT NAMES python3 python)
    if(PYTHON3_FOR_LINT)
        add_test(NAME undo_snapshot_lint
                 COMMAND "${PYTHON3_FOR_LINT}"
                         "${CMAKE_CURRENT_SOURCE_DIR}/test/undo_snapshot_lint_test.py"
                         "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(undo_snapshot_lint PROPERTIES TIMEOUT 15 LABELS "lint")

        # plan.md/NEXT.md mechanical self-consistency: unique task IDs, DONE
        # counts match the summary line, DONE tasks cite a commit, the priority
        # queue never points at a DONE task, no machine-specific absolute paths.
        add_test(NAME plan_consistency
                 COMMAND "${PYTHON3_FOR_LINT}"
                         "${CMAKE_CURRENT_SOURCE_DIR}/test/validate_plan_consistency.py"
                         "${CMAKE_CURRENT_SOURCE_DIR}"
                         "${CMAKE_BINARY_DIR}")
        set_tests_properties(plan_consistency PROPERTIES TIMEOUT 30 LABELS "lint")

        # SYS-W5-01: machine-readable cross-layer MC3 field matrix. Extracts
        # field/attribute names from XSD, the C++ model, the XML reader/
        # writer, and the MCB reader/writer, and CI-fails if a field is
        # present in >=2 of those layers but missing from another (the
        # "added a field to the model but forgot the MCB writer" bug class).
        # See test/field_matrix.py's own docstring for exact scope,
        # normalization rules, and the allowlist mechanism for documented
        # intentional asymmetries.
        add_test(NAME field_matrix
                 COMMAND "${PYTHON3_FOR_LINT}"
                         "${CMAKE_CURRENT_SOURCE_DIR}/test/field_matrix.py"
                         "${CMAKE_CURRENT_SOURCE_DIR}")
        set_tests_properties(field_matrix PROPERTIES TIMEOUT 30 LABELS "lint")
    endif()

    add_test(
        NAME smoke_test
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh
                $<TARGET_FILE:MeshCraft>
                ${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(smoke_test PROPERTIES TIMEOUT 30 LABELS "render")

    # SYS-W12-02: smoke test for the new `--benchmark` headless mode (see
    # MeshCraftApplication_Benchmark.cpp) -- informational, same philosophy
    # as SYS-W12-01's own test/benchmark.py (checks it runs to completion
    # and prints every expected category, not that any timing is fast).
    add_test(
        NAME benchmark_editor
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/benchmark_editor_test.sh
                $<TARGET_FILE:MeshCraft>
                ${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(benchmark_editor PROPERTIES TIMEOUT 30 LABELS "perf")

    # AUD-058: real lifecycle verification -- runs the app end-to-end and
    # asserts BloomGL::leakCheck()/shadowDebug's shutdown self-check never
    # reports a surviving GL handle. Regression guard for s_bloom.cleanup()
    # having existed with zero call sites.
    find_program(PYTHON3_FOR_LEAK_TEST NAMES python3 python)
    if(PYTHON3_FOR_LEAK_TEST)
        add_test(
            NAME gl_shutdown_leak
            COMMAND "${PYTHON3_FOR_LEAK_TEST}"
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/gl_shutdown_leak_test.py
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(gl_shutdown_leak PROPERTIES TIMEOUT 30 LABELS "render")
    endif()

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

    # Regression guard: a <grid subdivisions_x="4096" subdivisions_z="4096"/>
    # is legal per-field (kMaxTessellation) but its PRODUCT used to rebuild
    # ~16.8M vertices from scratch every frame with no cache, hanging the
    # editor well past this TIMEOUT (confirmed: unpatched run did not finish
    # within 20s; drawGridDynamic now bails to a placeholder mesh before
    # allocating when cols*rows exceeds the uint16_t index range).
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/grid_stress.mc3.xml")
        add_test(
            NAME smoke_test_grid_stress
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/grid_stress.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(smoke_test_grid_stress PROPERTIES TIMEOUT 30 LABELS "render")
    endif()

    # AUD-072: the same class of regression as smoke_test_grid_stress above,
    # in drawExtrudeDynamic() -- path segments x cross_section segments
    # (each individually legal per kMaxTessellation) combine multiplicatively
    # and used to be rebuilt from scratch every frame before the
    # numVerts>65535 bailout discarded the result (confirmed: unpatched run
    # did not finish within 25s; the vertex-count budget is now checked
    # before any allocation).
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/extrude_stress.mc3.xml")
        add_test(
            NAME smoke_test_extrude_stress
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/smoke_test.sh
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/extrude_stress.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(smoke_test_extrude_stress PROPERTIES TIMEOUT 30 LABELS "render")
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

    # STAB-0521: verify no GL error leaks out of a full frame's render passes.
    # medieval_castle.mc3.xml is the richest fixture available (skybox,
    # emissive materials, many objects/gizmos) -- note that bloom/SSAO
    # themselves are pure runtime UI toggles (off by default, no CLI/
    # document hook), so this exercises the base 3D pass + skybox + gizmos
    # + ImGui, not the bloom/SSAO post-process passes specifically.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/medieval_castle.mc3.xml")
        add_test(
            NAME gl_state_leak_test
            COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test/gl_state_leak_test.sh
                    $<TARGET_FILE:MeshCraft>
                    ${CMAKE_CURRENT_SOURCE_DIR}/test/medieval_castle.mc3.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(gl_state_leak_test PROPERTIES TIMEOUT 30 LABELS "render")
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

        # SYS-W12-01 (Phase 1): CLI-level performance benchmarks (XML/MCB
        # convert, glTF export) against small/medium/large fixtures.
        # Informational only (see test/benchmark.py's own docstring for
        # why this isn't a pass/fail timing gate) -- always green unless a
        # tool actually crashes.
        if(TARGET mc3tomcb AND TARGET mc3togltf)
            add_test(
                NAME benchmark
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/benchmark.py"
                        "$<TARGET_FILE:mc3tomcb>"
                        "$<TARGET_FILE:mc3togltf>"
                        "${CMAKE_CURRENT_SOURCE_DIR}"
            )
            set_tests_properties(benchmark PROPERTIES TIMEOUT 60 LABELS "perf")
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

        # 2026-07-20 audit finding #5: BasicEffect's own GPU fog used to be
        # enabled unconditionally whenever doc.environment->fog existed,
        # ALWAYS using linear start/end regardless of the document's
        # declared mode -- double-applying fog in Linear mode and applying
        # a wrong extra linear contribution in Exponential mode (which has
        # no start/end concept). Verify start/end have zero effect in
        # Exponential mode via real pixel comparison of two otherwise-
        # identical scenes differing only in start/end.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/fog_exponential_a.mc3.xml")
            add_test(
                NAME fog_exponential_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/fog_exponential_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/fog_exponential_a.mc3.xml"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/fog_exponential_nofog.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(fog_exponential_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # 2026-07-20 audit finding #6: a camera authored with `rotation`
        # instead of `target` used to be silently ignored by both the
        # camera gizmo and Look-Through-Camera mode -- both always looked
        # at `target`'s unused {0,0,0} default. Verify via real pixel
        # sampling that the document's own camera (rendered via
        # --screenshot, the same code path Look-Through-Camera uses)
        # actually honors `rotation`.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/camera_rotation.mc3.xml")
            add_test(
                NAME camera_rotation_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/camera_rotation_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/camera_rotation.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(camera_rotation_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # AUD-084: Bloom post-processing (I6) migrated from a hand-rolled
        # raw-GL FBO+shader pipeline onto RenderTarget2D + ShaderEffect +
        # SpriteBatch -- no dedicated visual-correctness test existed
        # before (only AUD-058's GL-resource-leak check). Real pixel
        # sampling proves a halo point just outside the box's own
        # silhouette brightens with bloom on vs off, while a point further
        # out (beyond the blur kernel's reach) does not.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/bloom.mc3.xml")
            add_test(
                NAME bloom_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/bloom_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/bloom.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(bloom_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # AUD-087: material-preview swatch (D7) migrated from a hand-rolled
        # raw-GL FBO+shader pipeline onto RenderTarget2D + ShaderEffect +
        # SpriteBatch -- no dedicated test existed before (it's only ever
        # drawn inside an ImGui panel, no CLI/scene-file equivalent). Reuses
        # the existing light_shading.mc3.xml fixture -- any scene works,
        # since the swatch is driven entirely by the test-only hook's own
        # hardcoded color, not scene content.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/light_shading.mc3.xml")
            add_test(
                NAME matpreview_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/matpreview_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/light_shading.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(matpreview_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # AUD-088: Shadow Map Debug (I7) migrated from a hand-rolled raw-GL
        # FBO + depth-texture pair onto RenderTarget2D -- no dedicated test
        # existed before (it's only ever drawn inside an ImGui panel, no
        # CLI/scene-file equivalent, and even then only renders when the
        # scene has a cast_shadows directional light).
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/shadow_debug.mc3.xml")
            add_test(
                NAME shadowdebug_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/shadowdebug_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/shadow_debug.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(shadowdebug_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # 2026-07-20 audit finding #4: doc.lights had zero effect on live
        # shading (BasicEffect's fixed 3-point default rig was used
        # unconditionally regardless of what was authored) -- verify a
        # strong authored directional light actually reaches the rendered
        # pixels via real pixel sampling, with a no-lights control scene
        # proving the result isn't a default-rig coincidence.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/light_shading.mc3.xml")
            add_test(
                NAME light_shading_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/light_shading_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/light_shading.mc3.xml"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/light_shading_control.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(light_shading_test PROPERTIES TIMEOUT 30 LABELS "render")
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

        # STAB-0670: confirms the CSG live preview no longer silently treats
        # Torus/Capsule/IcoSphere cutters as empty (missing switch case in
        # buildManifoldTree()).
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_icosphere_cutter.mc3.xml")
            add_test(
                NAME csg_icosphere_cutter_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_icosphere_cutter_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_icosphere_cutter.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(csg_icosphere_cutter_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0662: confirms buildCapsule()'s pole-closure/cylinder-body fix
        # also resolved the NotManifold CSG failure it caused (STAB-0670).
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_capsule_cutter.mc3.xml")
            add_test(
                NAME csg_capsule_cutter_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_capsule_cutter_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_capsule_cutter.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(csg_capsule_cutter_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0700: confirms the explicit MeshGL::Merge() tolerance fix
        # resolved Torus's NotManifold CSG failure (Merge()'s own auto-
        # computed baseline tolerance was too tight for Torus's seam
        # vertices, unlike Capsule/IcoSphere which worked with the default).
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_torus_cutter.mc3.xml")
            add_test(
                NAME csg_torus_cutter_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_torus_cutter_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_torus_cutter.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(csg_torus_cutter_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0216: confirms the CSG mesh cache never grows unbounded — with
        # 150 unique-geometry CSG nodes (far more than the 128-entry cap), the
        # cache must stay bounded (full-clear-on-exceed policy), not grow to 150.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_cache_eviction.mc3.xml")
            add_test(
                NAME csg_cache_eviction_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_cache_eviction_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_cache_eviction.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            # TIMEOUT raised from 60s (AUDIT-0053): this test spawns a real
            # MeshCraft --screenshot process (xvfb-run + full GL rendering of
            # 150 CSG boolean evaluations), which is genuinely sensitive to
            # concurrent machine load, not just its own cost -- observed
            # taking 24s under light load and >60s under heavy concurrent
            # build/test activity in the same environment.
            set_tests_properties(csg_cache_eviction_test PROPERTIES TIMEOUT 180 LABELS "render")
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

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_material.mc3.xml")
            add_test(
                NAME svg_texture_viewport_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_texture_viewport_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_material.mc3.xml"
                        red
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(svg_texture_viewport_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_inline_material.mc3.xml")
            add_test(
                NAME svg_inline_texture_viewport_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_texture_viewport_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_inline_material.mc3.xml"
                        blue
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(svg_inline_texture_viewport_test PROPERTIES TIMEOUT 30 LABELS "render")
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

        # SYS-W14-03: confirms --screenshot writes a real, decodable PNG for
        # a ".png" path (magic bytes + IHDR width/height/bit-depth/color-type),
        # and that a ".ppm" path is still exactly the same raw PPM (P6) format
        # every other screenshot-based test in this suite depends on.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml")
            add_test(
                NAME png_screenshot_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/png_screenshot_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(png_screenshot_test PROPERTIES TIMEOUT 30 LABELS "render")
        endif()

        # STAB-0327: confirms prefs.ini is auto-created on first launch
        # (LoadContent() now calls savePrefs() if the file doesn't already
        # exist), not only after the user explicitly closes the Preferences
        # dialog.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml")
            add_test(
                NAME prefs_first_launch_test
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/prefs_first_launch_test.py"
                        $<TARGET_FILE:MeshCraft>
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/house.mc3.xml"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            )
            set_tests_properties(prefs_first_launch_test PROPERTIES TIMEOUT 30 LABELS "render")
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
            ${CMAKE_CURRENT_SOURCE_DIR}/src/MeshCraft
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
