# MinGW-w64 cross-compile toolchain for MeshCraft (Linux host -> Windows target).
#
# Usage:
#   cmake -S . -B build-windows \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake \
#       -DMESH_CRAFT_BUILD_TESTING=OFF -G Ninja
#
# Known-good for the CNA-free CLI tools (mc3togltf.exe, mc3tomcb.exe), which
# build and link cleanly. The full GUI editor (MeshCraft.exe) does not
# currently complete a MinGW build — see NEXT.md section 4 for the tracked
# cross-repo (../cna, ../sharp-runtime) blockers.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
