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

install(TARGETS mc3tomcb RUNTIME DESTINATION bin)
