cmake_minimum_required(VERSION 3.21)
project(mc3togltf VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --------------------------------------------------------------------------
# Manifold — CSG boolean mesh evaluation.
# When included from the root CMakeLists the target already exists; this
# guard avoids a double-fetch in standalone builds.
# --------------------------------------------------------------------------
if(NOT TARGET manifold)
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
endif()

# --------------------------------------------------------------------------
# Mc3 library (from parent repo)
# --------------------------------------------------------------------------
if(NOT TARGET Mc3)
    get_filename_component(_mc3_dir "${CMAKE_CURRENT_SOURCE_DIR}/../mc3" ABSOLUTE)
    add_subdirectory("${_mc3_dir}" mc3_dep)
endif()

# --------------------------------------------------------------------------
# tinygltf — header-only glTF 2.0 reader/writer.
# Guard avoids stamp-file conflicts when included from root CMakeLists.
# --------------------------------------------------------------------------
FetchContent_GetProperties(tinygltf)
if(NOT tinygltf_POPULATED)
    FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
        GIT_TAG        v2.9.3
    )
    set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(TINYGLTF_HEADER_ONLY          ON  CACHE BOOL "" FORCE)
    set(TINYGLTF_INSTALL              OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tinygltf)
endif()

# --------------------------------------------------------------------------
# tinyobjloader — header-only OBJ mesh loader.
# Guard avoids double-declare when already fetched by the root CMakeLists.
# --------------------------------------------------------------------------
FetchContent_GetProperties(tinyobjloader)
if(NOT tinyobjloader_POPULATED)
    FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG        v2.0.0rc13
    )
    set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tinyobjloader)
endif()

# --------------------------------------------------------------------------
# Static library — GltfExporter + MeshBuilder (used by mc3togltf AND MeshCraft)
# --------------------------------------------------------------------------
add_library(mc3togltf_lib STATIC
    src/MeshBuilder.cpp
    src/GltfExporter.cpp
    src/CsgEvaluator.cpp
)

target_include_directories(mc3togltf_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src   # exposes GltfExporter.hpp, MeshBuilder.hpp
)

# Marked SYSTEM so -Wall -Wextra on mc3togltf_lib doesn't surface warnings
# from vendored headers (e.g. tinygltf's bundled stb_image_write.h).
target_include_directories(mc3togltf_lib SYSTEM PUBLIC
    ${tinygltf_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
)

target_link_libraries(mc3togltf_lib PUBLIC Mc3 tinyobjloader manifold)

target_compile_options(mc3togltf_lib PRIVATE -Wall -Wextra)

# --------------------------------------------------------------------------
# Executable — thin wrapper around the library
# --------------------------------------------------------------------------
add_executable(mc3togltf src/main.cpp)

target_include_directories(mc3togltf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mc3togltf PRIVATE mc3togltf_lib)

install(TARGETS mc3togltf RUNTIME DESTINATION bin)

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
include(CTest)

if(BUILD_TESTING)
    find_program(PYTHON3_EXEC python3)
    if(PYTHON3_EXEC)
        set(_anim_xml        "${CMAKE_CURRENT_SOURCE_DIR}/../test/animation_test.mc3.xml")
        set(_house_xml       "${CMAKE_CURRENT_SOURCE_DIR}/../test/house.mc3.xml")
        set(_primitives_xml  "${CMAKE_CURRENT_SOURCE_DIR}/../test/all_primitives.mc3.xml")
        set(_objects_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/all_objects.mc3.xml")
        set(_csg_xml         "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_test.mc3.xml")
        set(_large_scene_xml          "${CMAKE_CURRENT_SOURCE_DIR}/../test/large_scene.mc3.xml")
        set(_csg_union_xml            "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_union_export.mc3.xml")
        set(_csg_diff_xml             "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_difference_export.mc3.xml")
        set(_csg_isect_xml            "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_intersection_export.mc3.xml")
        set(_csg_unsupported_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_unsupported_child.mc3.xml")
        set(_csg_nested_xml           "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_nested.mc3.xml")
        set(_csg_semantics_xml        "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_semantics.mc3.xml")
        set(_csg_mesh_child_xml       "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_mesh_child.mc3.xml")
        set(_csg_stress_xml           "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_stress.mc3.xml")
        set(_instance_deform_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/instance_deform_cache.mc3.xml")
        set(_close_dimensions_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/close_dimensions.mc3.xml")
        set(_geom_cache_key_xml       "${CMAKE_CURRENT_SOURCE_DIR}/../test/geom_cache_key.mc3.xml")
        set(_empty_definition_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/empty_definition.mc3.xml")
        set(_multi_child_def_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/multi_child_definition.mc3.xml")
        set(_obj_robustness_xml       "${CMAKE_CURRENT_SOURCE_DIR}/../test/obj_robustness.mc3.xml")
        set(_texture_sampler_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/texture_sampler.mc3.xml")
        set(_material_pbr_xml         "${CMAKE_CURRENT_SOURCE_DIR}/../test/material_pbr.mc3.xml")
        set(_node_transform_xml       "${CMAKE_CURRENT_SOURCE_DIR}/../test/node_transform.mc3.xml")
        set(_empty_scene_xml          "${CMAKE_CURRENT_SOURCE_DIR}/../test/empty_scene.mc3.xml")
        set(_svg_material_xml         "${CMAKE_CURRENT_SOURCE_DIR}/../test/svg_material.mc3.xml")
        set(_anim_unsupported_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/animation_unsupported.mc3.xml")
        set(_instance_variant_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/instance_variant.mc3.xml")
        set(_glb_texture_embed_xml    "${CMAKE_CURRENT_SOURCE_DIR}/../test/glb_texture_embed.mc3.xml")
        set(_scene_with_include_xml   "${CMAKE_CURRENT_SOURCE_DIR}/../test/scene_with_include.mc3.xml")
        set(_embed_mesh_source_xml    "${CMAKE_CURRENT_SOURCE_DIR}/../test/embed_mesh_source.mc3.xml")
        if(EXISTS "${_anim_xml}" AND EXISTS "${_house_xml}")
            add_test(
                NAME mc3togltf_gltf
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/gltf_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_anim_xml}"
                        "${_house_xml}"
            )
            set_tests_properties(mc3togltf_gltf PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_primitives_xml}")
            add_test(
                NAME mc3togltf_all_primitives
                COMMAND "$<TARGET_FILE:mc3togltf>"
                        "${_primitives_xml}"
                        "${CMAKE_CURRENT_BINARY_DIR}/all_primitives_test.glb"
            )
            set_tests_properties(mc3togltf_all_primitives PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_primitives_xml}" AND EXISTS "${_objects_xml}")
            add_test(
                NAME mc3togltf_export_verification
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/all_primitives_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_primitives_xml}"
                        "${_objects_xml}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../test"
            )
            set_tests_properties(mc3togltf_export_verification PROPERTIES TIMEOUT 60 LABELS "export")
        endif()
        if(EXISTS "${_large_scene_xml}")
            add_test(
                NAME mc3togltf_large_scene
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/large_scene_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_large_scene_xml}"
            )
            set_tests_properties(mc3togltf_large_scene PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_csg_xml}")
            add_test(
                NAME mc3togltf_csg_strict
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_strict_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_xml}"
            )
            set_tests_properties(mc3togltf_csg_strict PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_csg_union_xml}" AND EXISTS "${_csg_diff_xml}" AND EXISTS "${_csg_isect_xml}")
            add_test(
                NAME mc3togltf_csg_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_union_xml}"
                        "${_csg_diff_xml}"
                        "${_csg_isect_xml}"
            )
            set_tests_properties(mc3togltf_csg_export PROPERTIES TIMEOUT 60 LABELS "export")
        endif()
        if(EXISTS "${_csg_unsupported_xml}")
            add_test(
                NAME mc3togltf_csg_unsupported
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_unsupported_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_unsupported_xml}"
            )
            set_tests_properties(mc3togltf_csg_unsupported PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_csg_nested_xml}")
            add_test(
                NAME mc3togltf_csg_nested
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_nested_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_nested_xml}"
            )
            set_tests_properties(mc3togltf_csg_nested PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_csg_semantics_xml}")
            add_test(
                NAME mc3togltf_csg_semantics
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_semantics_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_semantics_xml}"
            )
            set_tests_properties(mc3togltf_csg_semantics PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_csg_mesh_child_xml}")
            add_test(
                NAME mc3togltf_csg_mesh_child
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_mesh_child_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_mesh_child_xml}"
            )
            set_tests_properties(mc3togltf_csg_mesh_child PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_csg_stress_xml}")
            add_test(
                NAME mc3togltf_csg_stress
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_stress_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_stress_xml}"
            )
            set_tests_properties(mc3togltf_csg_stress PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_instance_deform_xml}")
            add_test(
                NAME mc3togltf_instance_deform_cache
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/instance_deform_cache_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_instance_deform_xml}"
            )
            set_tests_properties(mc3togltf_instance_deform_cache PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_close_dimensions_xml}")
            add_test(
                NAME mc3togltf_float_cache_key
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/float_cache_key_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_close_dimensions_xml}"
            )
            set_tests_properties(mc3togltf_float_cache_key PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_geom_cache_key_xml}")
            add_test(
                NAME mc3togltf_geom_cache_key
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/geom_cache_key_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_geom_cache_key_xml}"
            )
            set_tests_properties(mc3togltf_geom_cache_key PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_empty_definition_xml}" AND EXISTS "${_multi_child_def_xml}")
            add_test(
                NAME mc3togltf_definition_instancing
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/definition_instancing_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_empty_definition_xml}"
                        "${_multi_child_def_xml}"
            )
            set_tests_properties(mc3togltf_definition_instancing PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_obj_robustness_xml}")
            add_test(
                NAME mc3togltf_obj_robustness
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/obj_robustness_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_obj_robustness_xml}"
            )
            set_tests_properties(mc3togltf_obj_robustness PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_texture_sampler_xml}")
            add_test(
                NAME mc3togltf_texture_sampler
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/texture_sampler_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_texture_sampler_xml}"
            )
            set_tests_properties(mc3togltf_texture_sampler PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_material_pbr_xml}")
            add_test(
                NAME mc3togltf_material_pbr
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/material_pbr_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_material_pbr_xml}"
            )
            set_tests_properties(mc3togltf_material_pbr PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_node_transform_xml}")
            add_test(
                NAME mc3togltf_node_transform
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/node_transform_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_node_transform_xml}"
            )
            set_tests_properties(mc3togltf_node_transform PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_empty_scene_xml}")
            add_test(
                NAME mc3togltf_empty_scene
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/empty_scene_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_empty_scene_xml}"
            )
            set_tests_properties(mc3togltf_empty_scene PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_house_xml}" AND EXISTS "${_obj_robustness_xml}")
            add_test(
                NAME mc3togltf_export_stats
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/export_stats_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_house_xml}"
                        "${_obj_robustness_xml}"
            )
            set_tests_properties(mc3togltf_export_stats PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_house_xml}" AND EXISTS "${_primitives_xml}")
            add_test(
                NAME mc3togltf_cli_extension
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/cli_extension_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_house_xml}"
                        "${_primitives_xml}"
            )
            set_tests_properties(mc3togltf_cli_extension PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_glb_texture_embed_xml}")
            add_test(
                NAME mc3togltf_glb_texture_embed
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/glb_texture_embed_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_glb_texture_embed_xml}"
            )
            set_tests_properties(mc3togltf_glb_texture_embed PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_house_xml}")
            add_test(
                NAME mc3togltf_no_partial_output
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/no_partial_output_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_house_xml}"
            )
            set_tests_properties(mc3togltf_no_partial_output PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_house_xml}")
            add_test(
                NAME mc3togltf_determinism
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/determinism_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_house_xml}"
            )
            set_tests_properties(mc3togltf_determinism PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        add_test(
            NAME mc3togltf_golden
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/golden_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_golden PROPERTIES TIMEOUT 30 LABELS "export")
        add_test(
            NAME mc3togltf_help_text
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/help_text_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_help_text PROPERTIES TIMEOUT 30 LABELS "export")
        if(EXISTS "${_scene_with_include_xml}")
            add_test(
                NAME mc3togltf_include_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/include_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_scene_with_include_xml}"
            )
            set_tests_properties(mc3togltf_include_export PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_embed_mesh_source_xml}")
            add_test(
                NAME mc3togltf_embed_mesh_source
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/embed_mesh_source_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_embed_mesh_source_xml}"
            )
            set_tests_properties(mc3togltf_embed_mesh_source PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_svg_material_xml}")
            add_test(
                NAME mc3togltf_svg_texture_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_texture_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_svg_material_xml}"
            )
            set_tests_properties(mc3togltf_svg_texture_export PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_anim_unsupported_xml}")
            add_test(
                NAME mc3togltf_animation_unsupported
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/animation_unsupported_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_anim_unsupported_xml}"
            )
            set_tests_properties(mc3togltf_animation_unsupported PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_instance_variant_xml}")
            add_test(
                NAME mc3togltf_instance_variant
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/instance_variant_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_instance_variant_xml}"
            )
            set_tests_properties(mc3togltf_instance_variant PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        add_test(
            NAME mc3togltf_large_scene_generated
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/large_scene_generated_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_large_scene_generated PROPERTIES TIMEOUT 60 LABELS "export")

        # STAB-0244: 500-object export, asserting completion in < 30s
        # (same script, scale=2.5 -> 250+125+125 = 500 objects).
        add_test(
            NAME mc3togltf_large_scene_500
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/large_scene_generated_test.py"
                    "$<TARGET_FILE:mc3togltf>"
                    "2.5"
        )
        set_tests_properties(mc3togltf_large_scene_500 PROPERTIES TIMEOUT 60 LABELS "export")
    endif()
endif()
