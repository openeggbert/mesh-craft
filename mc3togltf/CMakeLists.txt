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

# NanoSVG is a pair of zlib-licensed headers. SvgRasterizer.cpp compiles its
# implementation once and shares the resulting CPU RGBA image with both the
# glTF exporter and the CNA editor viewport.
FetchContent_Declare(nanosvg
    GIT_REPOSITORY https://github.com/memononen/nanosvg.git
    GIT_TAG        25241c5a8f8451d41ab1b02ab2d865b01600d949
)
FetchContent_MakeAvailable(nanosvg)

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
    src/GltfImporter.cpp
    src/CsgEvaluator.cpp
    src/SvgRasterizer.cpp
)

target_include_directories(mc3togltf_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src   # exposes GltfExporter.hpp, MeshBuilder.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../include  # shared CNA-free asset LOD policy
)

# Marked SYSTEM so -Wall -Wextra on mc3togltf_lib doesn't surface warnings
# from vendored headers (e.g. tinygltf's bundled stb_image_write.h).
target_include_directories(mc3togltf_lib SYSTEM PUBLIC
    ${tinygltf_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
    ${nanosvg_SOURCE_DIR}/src
)

target_link_libraries(mc3togltf_lib PUBLIC Mc3 tinyobjloader manifold)

target_compile_options(mc3togltf_lib PRIVATE -Wall -Wextra)
if(COMMAND meshcraft_apply_sanitize)
    meshcraft_apply_sanitize(mc3togltf_lib)
endif()

# --------------------------------------------------------------------------
# Executable — thin wrapper around the library
# --------------------------------------------------------------------------
add_executable(mc3togltf src/main.cpp)

target_include_directories(mc3togltf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mc3togltf PRIVATE mc3togltf_lib)

# AUD-054: the executable itself (a thin main.cpp wrapper) previously had
# no warning flags; mc3togltf_lib above already did.
target_compile_options(mc3togltf PRIVATE -Wall -Wextra)
if(COMMAND meshcraft_apply_sanitize)
    meshcraft_apply_sanitize(mc3togltf)
endif()

install(TARGETS mc3togltf RUNTIME DESTINATION bin)

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
include(CTest)

if(BUILD_TESTING)
    # C++ unit test for the ear-clipping polygon triangulator (concave caps).
    add_executable(mc3togltf_earclip_test test/earclip_test.cpp)
    target_link_libraries(mc3togltf_earclip_test PRIVATE mc3togltf_lib)
    target_include_directories(mc3togltf_earclip_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME mc3togltf_earclip COMMAND mc3togltf_earclip_test)
    set_tests_properties(mc3togltf_earclip PROPERTIES TIMEOUT 15 LABELS "export")

    # SYS-W14-35: direct parser/import coverage (multi-usemtl splitting,
    # MTL subset mapping/warnings, missing MTL, hostile OBJ indices).
    add_executable(mc3togltf_obj_material_import_test test/obj_material_import_test.cpp)
    target_link_libraries(mc3togltf_obj_material_import_test PRIVATE mc3togltf_lib)
    target_include_directories(mc3togltf_obj_material_import_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME mc3togltf_obj_material_import
             COMMAND mc3togltf_obj_material_import_test "${CMAKE_CURRENT_SOURCE_DIR}/../test")
    set_tests_properties(mc3togltf_obj_material_import PROPERTIES TIMEOUT 15 LABELS "export")

    # SYS-W14-36: direct self-contained GLB import, explicit trusted .gltf
    # conversion, unsupported-mode rejection, and MC3 -> GLB round trip.
    add_executable(mc3togltf_gltf_import_test test/gltf_import_test.cpp)
    target_link_libraries(mc3togltf_gltf_import_test PRIVATE mc3togltf_lib)
    target_include_directories(mc3togltf_gltf_import_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME mc3togltf_gltf_import COMMAND mc3togltf_gltf_import_test)
    set_tests_properties(mc3togltf_gltf_import PROPERTIES TIMEOUT 20 LABELS "export")

    # SYS-W14-37: quantization accessors/extension, GLB-vs-glTF image
    # behavior, deterministic output, estimate, and object-id report.
    add_executable(mc3togltf_export_optimization_test test/gltf_export_optimization_test.cpp)
    target_link_libraries(mc3togltf_export_optimization_test PRIVATE mc3togltf_lib)
    target_include_directories(mc3togltf_export_optimization_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME mc3togltf_export_optimization COMMAND mc3togltf_export_optimization_test)
    set_tests_properties(mc3togltf_export_optimization PROPERTIES TIMEOUT 20 LABELS "export")

    # SYS-W14-38: named ranges, rate/reverse baking, explicit core-vs-extras
    # policy, deterministic output, and per-channel object-id diagnostics.
    add_executable(mc3togltf_animation_clip_export_test test/animation_clip_export_test.cpp)
    target_link_libraries(mc3togltf_animation_clip_export_test PRIVATE mc3togltf_lib)
    target_include_directories(mc3togltf_animation_clip_export_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME mc3togltf_animation_clip_export COMMAND mc3togltf_animation_clip_export_test)
    set_tests_properties(mc3togltf_animation_clip_export PROPERTIES TIMEOUT 20 LABELS "export")

    add_executable(mc3togltf_svg_rasterizer_test test/svg_rasterizer_test.cpp)
    target_link_libraries(mc3togltf_svg_rasterizer_test PRIVATE mc3togltf_lib)
    target_include_directories(mc3togltf_svg_rasterizer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME mc3togltf_svg_rasterizer COMMAND mc3togltf_svg_rasterizer_test)
    set_tests_properties(mc3togltf_svg_rasterizer PROPERTIES TIMEOUT 15 LABELS "export")

    # SYS-W1-01 (pre-export integration point): GltfExporter::validation,
    # populated for documents that never went through a parser load at all.
    add_executable(mc3togltf_pre_export_validation_test test/pre_export_validation_test.cpp)
    target_link_libraries(mc3togltf_pre_export_validation_test PRIVATE mc3togltf_lib)
    target_include_directories(mc3togltf_pre_export_validation_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME mc3togltf_pre_export_validation COMMAND mc3togltf_pre_export_validation_test)
    # F28 (2026-07-20 audit): this test's own 5000-segment sphere fixture
    # (used to exercise the hostile-tessellation-value diagnostic) measured
    # ~22-25s to tessellate on a fresh unoptimized (Debug) build -- past the
    # 15s TIMEOUT this test had, a deterministic failure, not machine-load-
    # related. 60s matches the convention this file already uses for its
    # other heavier tests (mc3togltf_csg_export, mc3togltf_export_verification).
    set_tests_properties(mc3togltf_pre_export_validation PROPERTIES TIMEOUT 60 LABELS "export")

    # NEXT.md task (2026-07-19): CsgEvaluator.cpp's Intersection case null
    # first-child guard. Built the same way as pre_export_validation_test.cpp
    # above -- an Mc3Object tree constructed programmatically, since no
    # parser path produces a null child.
    add_executable(mc3togltf_csg_null_child_test test/csg_null_child_test.cpp)
    target_link_libraries(mc3togltf_csg_null_child_test PRIVATE mc3togltf_lib)
    target_include_directories(mc3togltf_csg_null_child_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME mc3togltf_csg_null_child COMMAND mc3togltf_csg_null_child_test)
    set_tests_properties(mc3togltf_csg_null_child PROPERTIES TIMEOUT 15 LABELS "export")

    find_program(PYTHON3_EXEC python3)
    find_program(BLENDER_EXEC blender)
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
        set(_csg_shading_materials_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_shading_materials.mc3.xml")
        set(_instance_deform_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/instance_deform_cache.mc3.xml")
        set(_close_dimensions_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/close_dimensions.mc3.xml")
        set(_geom_cache_key_xml       "${CMAKE_CURRENT_SOURCE_DIR}/../test/geom_cache_key.mc3.xml")
        set(_empty_definition_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/empty_definition.mc3.xml")
        set(_multi_child_def_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/multi_child_definition.mc3.xml")
        set(_anim_geom_cache_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/anim_geom_cache.mc3.xml")
        add_test(
            NAME mc3togltf_large_scene_glb_size
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/large_scene_glb_size_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_large_scene_glb_size PROPERTIES TIMEOUT 30 LABELS "export")
        if(EXISTS "${_anim_geom_cache_xml}")
            add_test(
                NAME mc3togltf_anim_geom_cache
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/anim_geom_cache_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_anim_geom_cache_xml}"
            )
            set_tests_properties(mc3togltf_anim_geom_cache PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        # STAB-0252: real headless Blender import of a generated large scene.
        # Optional external tooling -- only registered if `blender` is found.
        if(BLENDER_EXEC)
            add_test(
                NAME mc3togltf_blender_import
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/blender_import_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${BLENDER_EXEC}"
            )
            set_tests_properties(mc3togltf_blender_import PROPERTIES TIMEOUT 90 LABELS "export")

            # STAB-0431: a single-material box's PBR values survive export ->
            # Blender import intact (Base Color / Roughness / Metallic read
            # back from the imported Principled BSDF node).
            add_test(
                NAME mc3togltf_material_pbr_blender_import
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/material_pbr_blender_import_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${BLENDER_EXEC}"
            )
            set_tests_properties(mc3togltf_material_pbr_blender_import PROPERTIES TIMEOUT 90 LABELS "export")

            # STAB-0642: a real, richly-authored release-representative scene
            # (not a synthetic generated one) survives export -> Blender import.
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../test/medieval_castle.mc3.xml")
                add_test(
                    NAME mc3togltf_release_sample_blender_import
                    COMMAND "${PYTHON3_EXEC}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/test/release_sample_blender_import_test.py"
                            "$<TARGET_FILE:mc3togltf>"
                            "${BLENDER_EXEC}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/../test/medieval_castle.mc3.xml"
                )
                set_tests_properties(mc3togltf_release_sample_blender_import PROPERTIES TIMEOUT 90 LABELS "export")
            endif()
        endif()
        # STAB-0257: >1M-triangle OBJ export stress test. Generates the OBJ
        # on the fly (not a committed static fixture -- it's ~28MB as text).
        add_test(
            NAME mc3togltf_large_obj_stress
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/large_obj_stress_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_large_obj_stress PROPERTIES TIMEOUT 90 LABELS "export")
        set(_obj_robustness_xml       "${CMAKE_CURRENT_SOURCE_DIR}/../test/obj_robustness.mc3.xml")
        set(_obj_material_groups_xml  "${CMAKE_CURRENT_SOURCE_DIR}/../test/obj_material_groups.mc3.xml")
        set(_texture_sampler_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/texture_sampler.mc3.xml")
        set(_color_space_export_xml   "${CMAKE_CURRENT_SOURCE_DIR}/../test/color_space_export.mc3.xml")
        set(_material_pbr_xml         "${CMAKE_CURRENT_SOURCE_DIR}/../test/material_pbr.mc3.xml")
        set(_node_transform_xml       "${CMAKE_CURRENT_SOURCE_DIR}/../test/node_transform.mc3.xml")
        set(_empty_scene_xml          "${CMAKE_CURRENT_SOURCE_DIR}/../test/empty_scene.mc3.xml")
        set(_svg_material_xml         "${CMAKE_CURRENT_SOURCE_DIR}/../test/svg_material.mc3.xml")
        set(_svg_inline_material_xml  "${CMAKE_CURRENT_SOURCE_DIR}/../test/svg_inline_material.mc3.xml")
        set(_svg_invalid_material_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/svg_invalid_material.mc3.xml")
        set(_svg_large_material_xml   "${CMAKE_CURRENT_SOURCE_DIR}/../test/svg_large_material.mc3.xml")
        set(_anim_unsupported_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/animation_unsupported.mc3.xml")
        set(_instance_variant_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/instance_variant.mc3.xml")
        set(_asset_lod_export_xml     "${CMAKE_CURRENT_SOURCE_DIR}/../test/asset_lod_export.mc3.xml")
        set(_scene_semantics_export_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/scene_semantics_export.mc3.xml")
        set(_glb_texture_embed_xml    "${CMAKE_CURRENT_SOURCE_DIR}/../test/glb_texture_embed.mc3.xml")
        set(_scene_with_include_xml   "${CMAKE_CURRENT_SOURCE_DIR}/../test/scene_with_include.mc3.xml")
        set(_embed_mesh_source_xml    "${CMAKE_CURRENT_SOURCE_DIR}/../test/embed_mesh_source.mc3.xml")
        set(_object_metadata_xml      "${CMAKE_CURRENT_SOURCE_DIR}/../test/object_metadata_export.mc3.xml")
        set(_uv_mapping_xml           "${CMAKE_CURRENT_SOURCE_DIR}/../test/uv_mapping_export.mc3.xml")
        set(_light_export_xml         "${CMAKE_CURRENT_SOURCE_DIR}/../test/light_export_all_types.mc3.xml")
        if(EXISTS "${_light_export_xml}")
            add_test(
                NAME mc3togltf_light_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/light_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_light_export_xml}"
            )
            set_tests_properties(mc3togltf_light_export PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        add_test(
            NAME mc3togltf_environment_export
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/environment_export_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_environment_export PROPERTIES TIMEOUT 30 LABELS "export")
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
        if(EXISTS "${_csg_shading_materials_xml}")
            add_test(
                NAME mc3togltf_csg_shading_materials
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_shading_materials_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_shading_materials_xml}"
            )
            set_tests_properties(mc3togltf_csg_shading_materials PROPERTIES TIMEOUT 30 LABELS "export")
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
        if(EXISTS "${_obj_material_groups_xml}")
            add_test(
                NAME mc3togltf_obj_material_groups_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/obj_material_groups_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_obj_material_groups_xml}"
            )
            set_tests_properties(mc3togltf_obj_material_groups_export PROPERTIES TIMEOUT 30 LABELS "export")
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
        if(EXISTS "${_color_space_export_xml}")
            add_test(
                NAME mc3togltf_color_space_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/color_space_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_color_space_export_xml}"
            )
            set_tests_properties(mc3togltf_color_space_export PROPERTIES TIMEOUT 30 LABELS "export")
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
        set(_glb_texture_embed_jpeg_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/glb_texture_embed_jpeg.mc3.xml")
        if(EXISTS "${_glb_texture_embed_jpeg_xml}")
            add_test(
                NAME mc3togltf_glb_texture_embed_jpeg
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/glb_texture_embed_jpeg_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_glb_texture_embed_jpeg_xml}"
            )
            set_tests_properties(mc3togltf_glb_texture_embed_jpeg PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        set(_normal_map_tangent_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/normal_map_tangent.mc3.xml")
        if(EXISTS "${_normal_map_tangent_xml}")
            add_test(
                NAME mc3togltf_tangent_generation
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/tangent_generation_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_normal_map_tangent_xml}"
            )
            set_tests_properties(mc3togltf_tangent_generation PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        set(_rotation_convention_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/rotation_convention_export.mc3.xml")
        if(EXISTS "${_rotation_convention_xml}")
            add_test(
                NAME mc3togltf_rotation_convention
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/rotation_convention_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_rotation_convention_xml}"
            )
            set_tests_properties(mc3togltf_rotation_convention PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        set(_coordinate_system_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/coordinate_system_export.mc3.xml")
        if(EXISTS "${_coordinate_system_xml}")
            add_test(
                NAME mc3togltf_coordinate_system
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/coordinate_system_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_coordinate_system_xml}"
            )
            set_tests_properties(mc3togltf_coordinate_system PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        set(_duplicate_name_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/duplicate_object_name_anim.mc3.xml")
        if(EXISTS "${_duplicate_name_xml}")
            add_test(
                NAME mc3togltf_duplicate_object_name
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/duplicate_object_name_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_duplicate_name_xml}"
            )
            set_tests_properties(mc3togltf_duplicate_object_name PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        add_test(
            NAME mc3togltf_degenerate_cone
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/degenerate_cone_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_degenerate_cone PROPERTIES TIMEOUT 30 LABELS "export")
        add_test(
            NAME mc3togltf_degenerate_polyline_extrude
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/degenerate_polyline_extrude_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_degenerate_polyline_extrude PROPERTIES TIMEOUT 30 LABELS "export")
        set(_csg_root_material_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/csg_root_material.mc3.xml")
        if(EXISTS "${_csg_root_material_xml}")
            add_test(
                NAME mc3togltf_csg_root_material
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/csg_root_material_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_root_material_xml}"
            )
            set_tests_properties(mc3togltf_csg_root_material PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        add_test(
            NAME mc3togltf_dangling_material
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/dangling_material_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_dangling_material PROPERTIES TIMEOUT 30 LABELS "export")
        add_test(
            NAME mc3togltf_event_bindings_omitted
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/event_bindings_export_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_event_bindings_omitted PROPERTIES TIMEOUT 30 LABELS "export")
        set(_extrude_sides_bezier_xml "${CMAKE_CURRENT_SOURCE_DIR}/../test/extrude_sides_bezier.mc3.xml")
        if(EXISTS "${_extrude_sides_bezier_xml}")
            add_test(
                NAME mc3togltf_extrude_sides_bezier
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/extrude_sides_bezier_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_extrude_sides_bezier_xml}"
            )
            set_tests_properties(mc3togltf_extrude_sides_bezier PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        add_test(
            NAME mc3togltf_dangling_animation_target
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/dangling_animation_target_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_dangling_animation_target PROPERTIES TIMEOUT 30 LABELS "export")
        add_test(
            NAME mc3togltf_icosphere_uv_seam
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/icosphere_uv_seam_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_icosphere_uv_seam PROPERTIES TIMEOUT 30 LABELS "export")
        # Hostile/degenerate geometry: cyclic <instance> (unbounded recursion)
        # and a zero-radius helix (NaN vertices) must fail loudly, not crash or
        # emit an invalid glTF reported as success.
        add_test(
            NAME mc3togltf_hostile_geometry
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/hostile_geometry_test.py"
                    "$<TARGET_FILE:mc3togltf>"
        )
        set_tests_properties(mc3togltf_hostile_geometry PROPERTIES TIMEOUT 40 LABELS "export")
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
                NAME mc3togltf_multi_file_partial_failure
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/multi_file_partial_failure_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_house_xml}"
            )
            set_tests_properties(mc3togltf_multi_file_partial_failure PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_primitives_xml}")
            add_test(
                NAME mc3togltf_glb_padding
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/glb_padding_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_primitives_xml}"
            )
            set_tests_properties(mc3togltf_glb_padding PROPERTIES TIMEOUT 30 LABELS "export")
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
        # STAB-0688: the determinism check above only ever exercised
        # house.mc3.xml (no CSG, animation, or textures) -- floating-point-
        # sensitive Manifold boolean ops, keyframe/interpolation evaluation,
        # and texture/image-byte embedding were completely unverified for
        # repeat-export reproducibility. Same script, three more fixtures.
        if(EXISTS "${_csg_xml}")
            add_test(
                NAME mc3togltf_determinism_csg
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/determinism_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_csg_xml}"
            )
            set_tests_properties(mc3togltf_determinism_csg PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_anim_xml}")
            add_test(
                NAME mc3togltf_determinism_animation
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/determinism_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_anim_xml}"
            )
            set_tests_properties(mc3togltf_determinism_animation PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_glb_texture_embed_xml}")
            add_test(
                NAME mc3togltf_determinism_texture
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/determinism_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_glb_texture_embed_xml}"
            )
            set_tests_properties(mc3togltf_determinism_texture PROPERTIES TIMEOUT 30 LABELS "export")
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
        if(EXISTS "${_object_metadata_xml}")
            add_test(
                NAME mc3togltf_object_metadata_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/object_metadata_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_object_metadata_xml}"
            )
            set_tests_properties(mc3togltf_object_metadata_export PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_uv_mapping_xml}")
            add_test(
                NAME mc3togltf_uv_mapping_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/uv_mapping_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_uv_mapping_xml}"
            )
            set_tests_properties(mc3togltf_uv_mapping_export PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_svg_material_xml}" AND EXISTS "${_svg_inline_material_xml}")
            add_test(
                NAME mc3togltf_svg_texture_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_texture_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_svg_material_xml}"
                        "${_svg_inline_material_xml}"
            )
            set_tests_properties(mc3togltf_svg_texture_export PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_svg_invalid_material_xml}" AND EXISTS "${_svg_large_material_xml}")
            add_test(
                NAME mc3togltf_svg_texture_safety
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/svg_texture_safety_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_svg_invalid_material_xml}"
                        "${_svg_large_material_xml}"
            )
            set_tests_properties(mc3togltf_svg_texture_safety PROPERTIES TIMEOUT 30 LABELS "export")
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
        if(EXISTS "${_asset_lod_export_xml}")
            add_test(
                NAME mc3togltf_asset_lod_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/asset_lod_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_asset_lod_export_xml}"
            )
            set_tests_properties(mc3togltf_asset_lod_export PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
        if(EXISTS "${_scene_semantics_export_xml}")
            add_test(
                NAME mc3togltf_scene_semantics_export
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/scene_semantics_export_test.py"
                        "$<TARGET_FILE:mc3togltf>"
                        "${_scene_semantics_export_xml}"
            )
            set_tests_properties(mc3togltf_scene_semantics_export PROPERTIES TIMEOUT 30 LABELS "export")
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

        # STAB-0245: 1000-object stress export (same script, scale=5.0 ->
        # 500+250+250 = 1000 objects), timeout 120s per the row's own ask.
        # Measured manually (not asserted in-test): ~0.1s wall time, ~8.5MB
        # peak RSS for the mc3togltf process — far under the row's 500MB bound.
        add_test(
            NAME mc3togltf_large_scene_1000
            COMMAND "${PYTHON3_EXEC}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test/large_scene_generated_test.py"
                    "$<TARGET_FILE:mc3togltf>"
                    "5.0"
        )
        set_tests_properties(mc3togltf_large_scene_1000 PROPERTIES TIMEOUT 120 LABELS "export")
    endif()
endif()
