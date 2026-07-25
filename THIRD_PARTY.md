# Third-Party Dependencies

MeshCraft itself is MIT-licensed (see `LICENSE`). This file lists every
third-party dependency it builds against, how it's obtained, and its
license — verified against the actual `FetchContent_Declare`/
`find_package` calls in this repo's `CMakeLists.txt` files, not just
copied from memory.

## Fetched via CMake `FetchContent` (vendored at build time)

| Dependency | Version (`GIT_TAG`) | License | Declared in | Used for |
|------------|---------------------|---------|-------------|----------|
| [tinyxml2](https://github.com/leethomason/tinyxml2) | `10.0.0` | zlib | `mc3/CMakeLists.txt` | `.mc3.xml` parsing/writing (the `Mc3` library) |
| [Dear ImGui](https://github.com/ocornut/imgui) | `v1.91.6` | MIT | `CMakeLists.txt` | Editor UI (SDL3 + OpenGL ES 3 backends) |
| [Manifold](https://github.com/elalish/manifold) | `v3.0.0` | Apache-2.0 | `CMakeLists.txt`, `mc3togltf/CMakeLists.txt` | Real CSG boolean evaluation (union/difference/intersection) |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | `v2.0.0rc13` | MIT | `CMakeLists.txt`, `mc3togltf/CMakeLists.txt` | Loading `<mesh src="*.obj">` external meshes |
| [tinygltf](https://github.com/syoyo/tinygltf) | `v2.9.3` | MIT | `mc3togltf/CMakeLists.txt` | Writing `.gltf`/`.glb` output |
| [NanoSVG](https://github.com/memononen/nanosvg) | `25241c5a8f8451d41ab1b02ab2d865b01600d949` | zlib | `mc3togltf/CMakeLists.txt` | CPU rasterization of `<texture type="svg">` for the editor and glTF export |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | `v0.18.3` | MIT | `CMakeLists.txt` | HTTPS client for the AI Assistant's Claude API calls |
| [nlohmann/json](https://github.com/nlohmann/json) | `v3.11.3` | MIT | `mc3/CMakeLists.txt` | `.mc3.json` parsing/writing (`Mc3JsonParser`/`Mc3JsonWriter`, R109) |
| [Lua](https://github.com/lua/lua) | `v5.4.7` | MIT | `CMakeLists.txt` | Interpreter for `Mc3Script` (`type="lua"`) execution (`LuaScriptRunner`, SYS-W14-18) |
| [sol2](https://github.com/ThePhD/sol2) | `v3.3.0` | MIT | `CMakeLists.txt` | Header-only C++ binding over Lua, used by `LuaScriptRunner` |

Manifold and tinygltf each vendor their own further sub-dependencies
(e.g. Clipper2, stb_image) as part of their own build — not
individually itemized here; see each project's own repository for its
own third-party notices.

## System / `find_package` dependencies (not vendored)

| Dependency | License | Required for | Notes |
|------------|---------|--------------|-------|
| [SQLite3](https://sqlite.org/) | Public domain | Model Registry (`ModelRegistry.cpp`) | System package (`libsqlite3-dev` on Debian/Ubuntu); desktop builds only — stubbed out on Emscripten/Android |
| [OpenSSL](https://openssl.org/) | Apache-2.0 (3.x) | AI Assistant's HTTPS transport (via cpp-httplib) | System package; desktop builds only |
| [LibXml2](http://xmlsoft.org/) | MIT | AI-response XSD validation (`AiResponseAlgorithms.hpp`, STAB-0391) | System package; optional — degrades gracefully to no-op validation if absent |
| [zlib](https://zlib.net/) | zlib | MCB binary format compression (`McbFormat.hpp`'s `MCB_FLAG_COMPRESSED`, `mcb/CMakeLists.txt`, SYS-W14-25) | System package (`zlib1g-dev` on Debian/Ubuntu); optional — `saveToBinary(..., compress=true)` throws and reading a compressed file throws a distinct "requires zlib" error if absent, rather than either silently no-op'ing or hard-blocking the whole build |
| [SDL3](https://www.libsdl.org/) | zlib | Windowing/input/audio (via **CNA**, below) | Not fetched directly by this repo — provided as a build product of the sibling `cna` repo |

## Sibling repositories (not vendored, checked out alongside `mesh-craft/`)

| Repository | License | Provides |
|------------|---------|----------|
| `../cna` | Microsoft Public License (Ms-PL) | XNA-style C++ runtime (windowing, input, audio, graphics backend) the editor UI is built on |
| `../sharp-runtime` | MIT | Math library (`Matrix`, `Vector3`, etc.) `cna`'s backend depends on |

Per `CLAUDE.md`, neither sibling repo is modified from this repository —
a separate development effort owns them.

## Verifying this list stays current

If you add a new `FetchContent_Declare`/`find_package` call, add a row
here in the same change — this is the kind of drift the `mc3.xsd`
audit earlier in this project's history found the hard way for a
different file (see `CONTRIBUTING.md`'s "API change policy").
