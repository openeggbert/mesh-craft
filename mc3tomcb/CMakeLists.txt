cmake_minimum_required(VERSION 3.21)
project(mc3tomcb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------------
# Mc3 library (from parent repo)
# --------------------------------------------------------------------------
if(NOT TARGET Mc3)
    get_filename_component(_mc3_dir "${CMAKE_CURRENT_SOURCE_DIR}/../mc3" ABSOLUTE)
    add_subdirectory("${_mc3_dir}" mc3_dep)
endif()

# --------------------------------------------------------------------------
# Mcb library (from parent repo)
# --------------------------------------------------------------------------
if(NOT TARGET Mcb)
    get_filename_component(_mcb_dir "${CMAKE_CURRENT_SOURCE_DIR}/../mcb" ABSOLUTE)
    add_subdirectory("${_mcb_dir}" mcb_dep)
endif()

# --------------------------------------------------------------------------
# Executable
# --------------------------------------------------------------------------
add_executable(mc3tomcb
    src/main.cpp
)

target_link_libraries(mc3tomcb PRIVATE Mcb Mc3)
target_compile_definitions(mc3tomcb PRIVATE MESHCRAFT_VERSION="${PROJECT_VERSION}")

# AUD-054/AUD-055: previously had no warning flags.
if(MSVC)
    target_compile_options(mc3tomcb PRIVATE /W4)
else()
    target_compile_options(mc3tomcb PRIVATE -Wall -Wextra)
endif()
if(COMMAND meshcraft_apply_sanitize)
    meshcraft_apply_sanitize(mc3tomcb)
endif()

include(GNUInstallDirs)
install(TARGETS mc3tomcb
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT release)

# --------------------------------------------------------------------------
# Tests — STAB-0058: drive the CLI through mc3.xml -> mcb -> mc3.xml and back,
# asserting a deterministic, lossless round-trip.
# --------------------------------------------------------------------------
include(CTest)

if(BUILD_TESTING)
    find_program(PYTHON3_EXEC NAMES python3 python)
    if(PYTHON3_EXEC)
        set(_rt_fixtures
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/features.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/all_objects.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/all_primitives.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/house.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/n3_scripts.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/n4_sounds_music.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/n5_triggers.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/n6_scene_states.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/n7_meta.mc3.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../test/asset_metadata_library_import.mc3.xml"
        )
        set(_rt_existing)
        foreach(_fx IN LISTS _rt_fixtures)
            if(EXISTS "${_fx}")
                list(APPEND _rt_existing "${_fx}")
            endif()
        endforeach()
        if(_rt_existing)
            add_test(
                NAME mc3tomcb_roundtrip
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/mc3tomcb_roundtrip_test.py"
                        "$<TARGET_FILE:mc3tomcb>"
                        ${_rt_existing}
            )
            set_tests_properties(mc3tomcb_roundtrip PROPERTIES TIMEOUT 60 LABELS "export")
        endif()

        # STAB-0537/0538: missing input file / write-protected output path
        # each exit non-zero with an error message, not a crash or silent
        # success.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../test/house.mc3.xml")
            add_test(
                NAME mc3tomcb_error_handling
                COMMAND "${PYTHON3_EXEC}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/test/mc3tomcb_error_test.py"
                        "$<TARGET_FILE:mc3tomcb>"
                        "${CMAKE_CURRENT_SOURCE_DIR}/../test/house.mc3.xml"
            )
            set_tests_properties(mc3tomcb_error_handling PROPERTIES TIMEOUT 30 LABELS "export")
        endif()
    endif()
endif()
