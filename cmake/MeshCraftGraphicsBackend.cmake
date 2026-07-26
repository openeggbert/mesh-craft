# MeshCraft's platform-level CNA graphics-backend policy.
#
# Keep this small decision separate from the root CMakeLists.txt so it can be
# tested without an Android NDK.  The caller remains responsible for enabling
# the corresponding CNA backend targets.

function(meshcraft_select_graphics_backend requested_backend target_android target_emscripten out_var)
    string(TOUPPER "${requested_backend}" selected_backend)

    # Both Android and Emscripten need the GLES 3.0 / WebGL 2 path.  The
    # editor draws ImGui through CNA, but its scene and optional shader effects
    # still require the source-GLSL contract provided by EasyGL.  Do not force
    # either platform onto SDL_RENDERER: that backend is deliberately gated by
    # GraphicsBackendCheck and would make an Android editor reject itself at
    # startup.
    if(target_android OR target_emscripten)
        set(selected_backend "EASYGL")
    endif()

    set(${out_var} "${selected_backend}" PARENT_SCOPE)
endfunction()
