if(NOT DEFINED MC_RELEASE_BUILD_DIR OR NOT DEFINED MC_RELEASE_VERSION OR
   NOT DEFINED MC_RELEASE_SYSTEM)
    message(FATAL_ERROR
        "MC_RELEASE_BUILD_DIR, MC_RELEASE_VERSION and MC_RELEASE_SYSTEM are required")
endif()

set(_mc_release_prefix "${MC_RELEASE_BUILD_DIR}/meshcraft-cli-release-prefix")
set(_mc_release_archive
    "${MC_RELEASE_BUILD_DIR}/MeshCraft-${MC_RELEASE_VERSION}-${MC_RELEASE_SYSTEM}-cli.tar.gz")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MC_RELEASE_BUILD_DIR}"
            --prefix "${_mc_release_prefix}" --component release
    RESULT_VARIABLE _mc_install_result)
if(NOT _mc_install_result EQUAL 0)
    message(FATAL_ERROR "CLI release installation failed: ${_mc_install_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cfvz "${_mc_release_archive}"
            --format=gnutar -- bin include lib
    WORKING_DIRECTORY "${_mc_release_prefix}"
    RESULT_VARIABLE _mc_archive_result)
if(NOT _mc_archive_result EQUAL 0)
    message(FATAL_ERROR "CLI release archiving failed: ${_mc_archive_result}")
endif()

message(STATUS "Created CLI release archive: ${_mc_release_archive}")
