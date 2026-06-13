#!/usr/bin/env bash
# Build MeshCraft for the browser via Emscripten.
# Usage: ./build-web.sh [--clean]
#
# Requirements:
#   emsdk installed at ~/Downloads/emsdk  (or override EMSDK_PATH below)
#   emsdk activated: source ~/Downloads/emsdk/emsdk_env.sh
#
# Output: cmake-build-web/MeshCraft.html  (+ .js / .wasm / .data)

set -e

EMSDK_PATH="${EMSDK_PATH:-$HOME/Downloads/emsdk}"
BUILD_DIR="cmake-build-web"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Source emsdk env if emcc not yet on PATH
if ! command -v emcc &>/dev/null; then
    if [ -f "$EMSDK_PATH/emsdk_env.sh" ]; then
        source "$EMSDK_PATH/emsdk_env.sh"
    else
        echo "ERROR: emcc not found and emsdk not at $EMSDK_PATH" >&2
        echo "Set EMSDK_PATH or run: source ~/Downloads/emsdk/emsdk_env.sh" >&2
        exit 1
    fi
fi

TOOLCHAIN="$EMSDK_PATH/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"

# --clean: wipe build dir and force full reconfigure + SDL3 rebuild
if [[ "$1" == "--clean" ]]; then
    echo "Cleaning $BUILD_DIR ..."
    rm -rf "$SCRIPT_DIR/$BUILD_DIR"
fi

echo "Configuring for Emscripten ..."
cmake -S "$SCRIPT_DIR" \
      -B "$SCRIPT_DIR/$BUILD_DIR" \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
      -DMESH_CRAFT_BUILD_TESTING=OFF

echo "Building ..."
cmake --build "$SCRIPT_DIR/$BUILD_DIR" --parallel

echo ""
echo "Done. To serve locally:"
echo "  cd $BUILD_DIR && python3 -m http.server 8080"
echo "  Open: http://localhost:8080/MeshCraft.html"
