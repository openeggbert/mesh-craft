cmake_minimum_required(VERSION 3.21)
project(MeshCraftPackageConsumer LANGUAGES CXX)

find_package(Mc3 0.1.0 CONFIG REQUIRED)
find_package(Mcb 0.1.0 CONFIG REQUIRED)

add_executable(meshcraft_package_consumer main.cpp)
target_compile_features(meshcraft_package_consumer PRIVATE cxx_std_23)
target_link_libraries(meshcraft_package_consumer PRIVATE
    MeshCraft::Mc3
    MeshCraft::Mcb)
