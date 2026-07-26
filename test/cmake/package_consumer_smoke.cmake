if(NOT DEFINED MC_PACKAGE_BUILD_DIR OR NOT DEFINED MC_PACKAGE_SOURCE_DIR)
    message(FATAL_ERROR "MC_PACKAGE_BUILD_DIR and MC_PACKAGE_SOURCE_DIR are required")
endif()

set(_mc_package_prefix "${MC_PACKAGE_BUILD_DIR}/package-consumer-release-prefix")
set(_mc_package_build "${MC_PACKAGE_BUILD_DIR}/package-consumer-release-build")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MC_PACKAGE_BUILD_DIR}"
            --prefix "${_mc_package_prefix}" --component release
    RESULT_VARIABLE _mc_install_result)
if(NOT _mc_install_result EQUAL 0)
    message(FATAL_ERROR "MeshCraft package installation failed: ${_mc_install_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${MC_PACKAGE_SOURCE_DIR}"
            -B "${_mc_package_build}"
            "-DCMAKE_PREFIX_PATH=${_mc_package_prefix}"
    RESULT_VARIABLE _mc_configure_result)
if(NOT _mc_configure_result EQUAL 0)
    message(FATAL_ERROR "External find_package configuration failed: ${_mc_configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_mc_package_build}" --parallel 2
    RESULT_VARIABLE _mc_build_result)
if(NOT _mc_build_result EQUAL 0)
    message(FATAL_ERROR "External package consumer build failed: ${_mc_build_result}")
endif()

execute_process(
    COMMAND "${_mc_package_build}/meshcraft_package_consumer"
    RESULT_VARIABLE _mc_run_result)
if(NOT _mc_run_result EQUAL 0)
    message(FATAL_ERROR "External package consumer failed: ${_mc_run_result}")
endif()
