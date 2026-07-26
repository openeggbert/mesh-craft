if(NOT DEFINED MC_RELEASE_BUILD_DIR OR NOT DEFINED MC_RELEASE_VERSION OR
   NOT DEFINED MC_RELEASE_SYSTEM OR NOT DEFINED MC_RELEASE_SOURCE_DIR)
    message(FATAL_ERROR
        "MC_RELEASE_BUILD_DIR, MC_RELEASE_VERSION, MC_RELEASE_SYSTEM and "
        "MC_RELEASE_SOURCE_DIR are required")
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

# SYS-W11-08: bundle notices/licenses directly in the archive rather than
# only in the source tree, so a consumer who only ever unpacks the archive
# (never clones the repo) still has them.
foreach(_mc_notice IN ITEMS LICENSE THIRD_PARTY.md)
    if(EXISTS "${MC_RELEASE_SOURCE_DIR}/${_mc_notice}")
        file(COPY "${MC_RELEASE_SOURCE_DIR}/${_mc_notice}" DESTINATION "${_mc_release_prefix}")
    endif()
endforeach()

# SYS-W11-08: a manifest of SHA-256 hashes for every installed file, so a
# consumer (or CI) can verify the archive's contents weren't corrupted or
# tampered with in transit, independent of trusting the archive tool itself.
# sha256sum-compatible format ("<hash>  <relative-path>") so `sha256sum -c`
# verifies it directly on Linux/macOS. Remove any manifest left behind by a
# PREVIOUS run of this script first: `cmake --install` never wipes the
# destination, so a stale SHA256SUMS.txt would otherwise be picked up by the
# glob below with its OLD content's hash, then get overwritten by this run's
# real manifest -- a self-referential mismatch on every second run.
file(REMOVE "${_mc_release_prefix}/SHA256SUMS.txt")
file(GLOB_RECURSE _mc_release_files LIST_DIRECTORIES false RELATIVE "${_mc_release_prefix}"
     "${_mc_release_prefix}/*")
list(SORT _mc_release_files)
set(_mc_manifest_lines "")
foreach(_mc_file IN LISTS _mc_release_files)
    file(SHA256 "${_mc_release_prefix}/${_mc_file}" _mc_file_hash)
    list(APPEND _mc_manifest_lines "${_mc_file_hash}  ${_mc_file}")
endforeach()
string(REPLACE ";" "\n" _mc_manifest_content "${_mc_manifest_lines}")
file(WRITE "${_mc_release_prefix}/SHA256SUMS.txt" "${_mc_manifest_content}\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cfvz "${_mc_release_archive}"
            --format=gnutar -- bin include lib LICENSE THIRD_PARTY.md SHA256SUMS.txt
    WORKING_DIRECTORY "${_mc_release_prefix}"
    RESULT_VARIABLE _mc_archive_result)
if(NOT _mc_archive_result EQUAL 0)
    message(FATAL_ERROR "CLI release archiving failed: ${_mc_archive_result}")
endif()

message(STATUS "Created CLI release archive: ${_mc_release_archive}")
