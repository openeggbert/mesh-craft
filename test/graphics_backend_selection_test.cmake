# Context-free regression test for MeshCraft's target-platform backend policy.
# It intentionally needs neither an Android NDK nor a graphics driver.

if(NOT DEFINED MeshCraft_SOURCE_DIR)
    message(FATAL_ERROR "MeshCraft_SOURCE_DIR must point at the repository root")
endif()

include("${MeshCraft_SOURCE_DIR}/cmake/MeshCraftGraphicsBackend.cmake")

function(assert_backend expected actual description)
    if(NOT "${expected}" STREQUAL "${actual}")
        message(FATAL_ERROR
            "${description}: expected '${expected}', got '${actual}'")
    endif()
    message(STATUS "PASS: ${description} (${actual})")
endfunction()

meshcraft_select_graphics_backend("SDL_RENDERER" TRUE FALSE android_backend)
assert_backend("EASYGL" "${android_backend}"
    "Android forces the GLES-capable EasyGL editor path")

meshcraft_select_graphics_backend("VULKAN" TRUE FALSE android_override_backend)
assert_backend("EASYGL" "${android_override_backend}"
    "Android cannot be switched to an unqualified backend")

meshcraft_select_graphics_backend("SDL_RENDERER" FALSE TRUE web_backend)
assert_backend("EASYGL" "${web_backend}"
    "Emscripten keeps its WebGL2 EasyGL path")

meshcraft_select_graphics_backend("VULKAN" FALSE FALSE desktop_backend)
assert_backend("VULKAN" "${desktop_backend}"
    "desktop preserves an explicitly requested backend")
