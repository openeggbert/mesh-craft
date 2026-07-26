if(NOT DEFINED MC_RELEASE_BUILD_DIR OR NOT DEFINED MC_RELEASE_SOURCE_DIR OR
   NOT DEFINED MC_RELEASE_VERSION OR NOT DEFINED MC_RELEASE_SYSTEM OR
   NOT DEFINED MC_RELEASE_PYTHON3)
    message(FATAL_ERROR
        "MC_RELEASE_BUILD_DIR, MC_RELEASE_SOURCE_DIR, MC_RELEASE_VERSION, "
        "MC_RELEASE_SYSTEM and MC_RELEASE_PYTHON3 are required")
endif()

# Build (or rebuild) the release archive fresh for this test, so a stale
# archive from an earlier configuration is never silently trusted.
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${MC_RELEASE_BUILD_DIR}"
            --target meshcraft_cli_release --parallel 2
    RESULT_VARIABLE _mc_build_result)
if(NOT _mc_build_result EQUAL 0)
    message(FATAL_ERROR "Building meshcraft_cli_release failed: ${_mc_build_result}")
endif()

set(_mc_archive
    "${MC_RELEASE_BUILD_DIR}/MeshCraft-${MC_RELEASE_VERSION}-${MC_RELEASE_SYSTEM}-cli.tar.gz")
if(NOT EXISTS "${_mc_archive}")
    message(FATAL_ERROR "Expected release archive not found: ${_mc_archive}")
endif()

# SYS-W11-08: extract OUTSIDE the build tree into a directory whose name
# contains both a space and a non-ASCII character, so the archive's
# usability is never accidentally coupled to a convenient path.
set(_mc_cleanroom "${MC_RELEASE_BUILD_DIR}/clean room café")
file(REMOVE_RECURSE "${_mc_cleanroom}")
file(MAKE_DIRECTORY "${_mc_cleanroom}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xfz "${_mc_archive}"
    WORKING_DIRECTORY "${_mc_cleanroom}"
    RESULT_VARIABLE _mc_extract_result)
if(NOT _mc_extract_result EQUAL 0)
    message(FATAL_ERROR "Extracting the release archive failed: ${_mc_extract_result}")
endif()

execute_process(
    COMMAND "${MC_RELEASE_PYTHON3}"
            "${MC_RELEASE_SOURCE_DIR}/test/clean_room_cli_release_test.py"
            "${_mc_cleanroom}"
            "${MC_RELEASE_SOURCE_DIR}/test/house.mc3.xml"
    RESULT_VARIABLE _mc_verify_result)
if(NOT _mc_verify_result EQUAL 0)
    message(FATAL_ERROR "Clean-room release verification failed: ${_mc_verify_result}")
endif()
