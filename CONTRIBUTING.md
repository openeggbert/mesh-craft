# Contributing to MeshCraft

This project ran a long **stabilization phase** (fully tested,
correctness-verified codebase before new features land) that is now
substantively complete — see `plan.md` for the active backlog
(`AUD-###`/`SYS-###` IDs) and `NEXT.md` for current status. The original
policy document is archived at `docs/history/STABILIZATION.md`.

## Build setup

See `README.md` for prerequisites and the standard build commands, and
`TESTING.md` for how to run the test suite. Two things trip people up
that aren't obvious from a normal CMake project:

### Adding a new `.cpp` file requires a reconfigure, not just a rebuild

Sources are collected with `file(GLOB_RECURSE)` in the root
`CMakeLists.txt`, so CMake does not notice a new `.cpp` file on its own.
After adding one — or registering a new standalone test executable in
`CMakeLists.txt` — re-run the configure step:

```sh
cmake -S . -B cmake-build-debug   # (or the CLion cmake binary — see below)
ninja -C cmake-build-debug
```

`ninja` alone will silently keep building the *old* file list.

The same applies to editing `mc3/mc3.xsd`: it's compiled into a
generated header (`Mc3XsdEmbed.hpp`, via `configure_file()` at CMake
*configure* time, not build time — see `NEXT.md`'s architecture notes).
Editing the schema and only rebuilding leaves the *old* schema content
linked into the binary. Always reconfigure after touching `mc3.xsd`.

### `cmake-build-debug/` must be reconfigured with CLion's bundled cmake

That directory was generated with CLion's bundled CMake (4.2.2), and
reconfiguring it with the system `cmake` (commonly an older version,
e.g. 3.31.6) fails during Generate with bogus `manifold`/`boolean3.cpp`
source errors — a toolchain-version issue, not a code problem. Use
CLion's cmake binary for this directory specifically:

```sh
CLION_CMAKE=/path/to/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON
```

A fresh build directory (e.g. `b-release/`) is fine with the system
cmake, as are the standalone `mc3`/`mcb`/`mc3togltf`/`mc3tomcb` builds.

## The CNA boundary

**CNA** (the SDL3/OpenGL runtime MeshCraft's editor UI is built on) and
**SHARP_RUNTIME** (the math library CNA depends on) are sibling repos
checked out next to `mesh-craft/`, brought in via
`add_subdirectory(../cna ...)`. They are **out of scope for changes made
from this repo**:

- **Do not modify CNA or SHARP_RUNTIME source files from this repo.** A
  separate development effort owns those repos. If a CNA/SHARP_RUNTIME
  bug blocks work here, document it (see `NEXT.md`'s "Current blocker"
  pattern) rather than patching it directly.
- **Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`**
  — this triggers a full CNA recompile, which is expensive and usually a
  sign the include path belongs somewhere else.
- Keep `mc3/`, `mcb/`, `mc3togltf/`, and `mc3tomcb/` **CNA-free and
  independently buildable**. None of them may gain a CNA/ImGui/SDL
  dependency. Verify with the standalone build commands in `TESTING.md`
  before submitting a change that touches any of those directories.
- Editor code (`src/MeshCraft/`) *is* CNA-coupled, but a surprising
  amount of its logic (command algorithms, undo/redo, parsing/
  validation) has **zero actual CNA dependency** once ImGui rendering
  and app-state bookkeeping are set aside. Where that's true, move the
  pure logic into a CNA-free header (suffixed `Alg` per function, e.g.
  `EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`) that both the real
  `.cpp` file and a headless test `#include` and call directly — no
  duplication. This is the established pattern for making
  CNA-adjacent code testable; use it rather than inventing a new one.

## API change policy

- **`Mc3Document`'s public API** (and its constituent types —
  `Mc3Object`, `Mc3Material`, `Mc3Texture`, etc.) is consumed by
  `mc3togltf`, `mc3tomcb`, the editor, and every test fixture in `test/`
  and `mc3/test/`. Before changing it: check `mc3togltf/src/*.cpp`,
  `mc3tomcb/src/*.cpp`, and the test XMLs in `test/` for anything that
  depends on the current shape, and update `mc3.xsd` in the same change
  if the XML representation changes (see "Schema/writer symmetry"
  below).
- **`mc3.xsd` must stay symmetric with `Mc3XmlWriter.cpp`.** If the
  writer starts emitting a new attribute or element, declare it in the
  schema in the same change — otherwise real, valid, round-tripping
  scenes will fail strict XSD validation (this happened for real: an
  audit found 8 separate writer-vs-schema gaps that had accumulated
  over time; see `plan.md`'s S2 section and `NEXT.md`'s history for
  specifics). When adding a new writer attribute/element, grep the
  schema for the containing type and add the declaration there before
  considering the feature done.
- **MCB format version** (`MCB_VERSION` in
  `mcb/include/MeshCraft/Mcb/McbFormat.hpp`) must be bumped on any
  breaking wire-format change (removing a tag, changing a tag's byte
  layout, etc.) — purely additive changes (a new optional field) don't
  require a bump as long as older readers can still skip what they
  don't recognize.
- **No CNA source changes** — see above.

## Workflow

- Every change should be scoped to what its active `AUD-###`/`SYS-###` row in
  `plan.md` actually asks for — no bundling unrelated cleanup into the same
  change.
- Add or update a test for the specific behavior you're changing;
  `plan.md`'s policy requires a task's status move to ✅ only once a
  test exists, is registered, runs, and passes.
- Update `NEXT.md` after a change lands — it's meant to always reflect
  current, verified state (see its own §10 "Resume prompt" for the
  expected update discipline).

## Code style (`SYS-W11-06`)

Checked-in `.clang-format`/`.clang-tidy` at the repo root, scoped to first-party
code (`mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/`, `src/MeshCraft/`,
`include/MeshCraft/`) — `../cna`/`../sharp-runtime` and every vendored
`FetchContent` dependency are excluded via `.clang-tidy`'s `HeaderFilterRegex`.
CI is active at `.github/workflows/ci.yml`, but neither tool is currently an
enforced formatter/linter job; run them locally:

```bash
# Format a file you touched (does not exist as a repo-wide pass — the
# existing 18.5k LOC was never mass-reformatted against this config, so a
# blind `-i` over the whole tree will produce a large, unreviewed diff)
clang-format -i path/to/your/file.cpp

# Lint first-party code (needs a compile database)
cmake -S . -B <build-dir> -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p <build-dir> path/to/your/file.cpp
```
