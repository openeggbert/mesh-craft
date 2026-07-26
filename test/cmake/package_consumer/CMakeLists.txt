cmake_minimum_required(VERSION 3.21)
project(MeshCraftPackageConsumer LANGUAGES CXX)

# SYS-W11-10: this smoke test simulates an external consumer, which would
# normally hardcode whatever version it depends on -- but pinning it to the
# same shared repo-root VERSION file avoids it silently going stale (and
# failing this very test) the next time that version changes.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../../VERSION" MESHCRAFT_VERSION_STRING LIMIT_COUNT 1)
find_package(Mc3 ${MESHCRAFT_VERSION_STRING} CONFIG REQUIRED)
find_package(Mcb ${MESHCRAFT_VERSION_STRING} CONFIG REQUIRED)

add_executable(meshcraft_package_consumer main.cpp)
target_compile_features(meshcraft_package_consumer PRIVATE cxx_std_23)
target_link_libraries(meshcraft_package_consumer PRIVATE
    MeshCraft::Mc3
    MeshCraft::Mcb)
