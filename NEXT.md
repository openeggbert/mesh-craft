# MeshCraft — Current Handoff

_Last updated: 2026-07-26._ Read this before resuming work. The authoritative
active queue is [`plan.md`](plan.md); historical detail is in
[`docs/history/`](docs/history/).

## Product snapshot

MeshCraft is a pre-production C++23 3D scene editor for the MC3 format. Its
canonical `Mc3Document` model is shared by XML/JSON parsing and writing, MCB,
the MC3→MCB and MC3→glTF/GLB tools, and the editor. The editor is built on CNA;
the sibling `cna` and `sharp-runtime` repositories are consumed read-only and
must not be modified here.

The feature set already includes:

- MC3 XML/JSON, MCB, glTF/GLB export, CLI conversion tools, validation, and
  bounded parser/resource limits;
- CSG, dynamic/generated geometry, authored LOD, materials, UV mapping,
  textures, lights, cameras, animation clips, and scene history;
- editable OBJ and self-contained GLB/glTF import, local model registries,
  asset packs, Lua scripts, triggers, states, and explicit event bindings;
- undo/redo, autosave recovery, and automated unit, differential, smoke,
  screenshot, and format round-trip coverage.

The original stabilization work and `SYS-W14-28` through `SYS-W14-39` are
complete. MeshCraft is now roughly 75–80% of the way to a solid 1.0: the work
remaining is concentrated in correctness, performance, documentation, CI, and
distribution rather than missing editor breadth.

## Current priorities

1. Obtain native standalone Windows qualification (`SYS-W11-06`). The
   release-readiness changes are published on `develop`; await the first
   remote `windows-2022` CTest/artifact evidence.

`SYS-W14-40` is complete: the explicit bounded Event Preview/Play mode is
covered by `event_preview_runner`. See `plan.md` for the remaining Windows
qualification evidence and the full release-gate order.

## Known release blockers and decisions

| Area | Live state |
| --- | --- |
| Alternate graphics backends | `SYS-W8-05` is blocked on CNA owner coordination and actual backend evidence. |
| Android | `AUD-042` is blocked on NDK/tooling, sibling-runtime cross-build repair, packaging, and device/emulator testing. |
| Undo retention | `AUD-038` deliberately limits undo to 20 deep-copy entries; `SYS-W9-05` targets duplicate ownership, not unlimited history. |
| Unknown MC3 XML | Current policy is lossy handling of unknown elements/attributes; do not add a preservation bag without an approved compatibility contract. |
| New features | Do not add formats, importers, skinning, Draco/meshopt, or broad modelling tools before the active quality queue. |

## Immediate technical risks

- Existing CI now has Clang ASan+UBSan and bounded XML/JSON/MCB/GLB fuzz
  smoke coverage for standalone code. `Mc3`/`Mcb` CMake packages and a
  relocatable Linux CLI archive are locally qualified; Windows standalone
  qualification has a dedicated `windows-2022` job and successful local
  MinGW 14 builds, but awaits its first runner CTest/artifact evidence.

## Documentation truth

`SYS-W13-02` is complete. [`docs/CAPABILITY_MATRIX.md`](docs/CAPABILITY_MATRIX.md)
is the authoritative short matrix for format, editor, exporter, and platform
scope. `capability_documentation` statically checks the source-backed Walk
Mode proxy, Web IDBFS, autosave-recovery, and ordinary-UV claims. It excludes
volatile test totals and unverified platform-runtime assertions.

`SYS-W2-01` is complete. Lua executes against an isolated document copy with a
16 MiB VM allocator cap and the existing 50-million-instruction ceiling. Only
a fully validated result commits; the editor creates undo/history at that
commit boundary, so a failed script cannot leave a partial edit or false dirty
state.

`SYS-W12-03` is complete. Ordinary tinting uses existing permanent geometry
and effect state, authored UV buffers are persistent/bounded by mesh and
mapping inputs, and the 100-instance benchmark fixture proves a warm draw has
zero ordinary tint/authored-UV creations while reusing the UV cache.

`SYS-W12-04` is complete. The benchmark now splits CPU primitive mesh
generation, CSG cold evaluation/warm lookup, and cache-miss texture
decode/upload from frame time, with allocation/cache counters and no fragile
wall-clock gate. The UV fixture and a CSG fixture both exercised those lines.

`SYS-W3-03` is complete. `RegistryWorkspace` owns the registry database and
all Registry UI workflow state; `MeshCraftApplication` now has a single
workspace member and provides only explicit scene-insert/status callbacks.
The controller lifecycle is CNA-free tested and existing registry-data tests
continue to cover SQLite and no-SQLite behavior.

`SYS-W3-04` is complete. `AutomationWorkspace` owns the shared non-serialized
Scripts/Triggers/States/Events selection, Lua, and Preview/Play state, while
all four renderers use narrow callbacks rather than reaching into
`MeshCraftApplication`. Trigger execution snapshots its steps before a Lua
transaction can swap the document, so later steps remain valid.

`SYS-W9-05` is complete. Matching undo and automatic-history entries share a
single immutable document snapshot, while each restore still produces an
independent writable document. The 1,000-object test proves one retained graph
instead of two and covers selection/undo/history equivalence.

`SYS-W11-04` is complete. CI runs all four CNA-free standalone components
under Clang ASan+UBSan, then runs fixed-seed, bounded libFuzzer smoke tests for
XML, JSON, MCB and self-contained GLB import. The source corpora stay
immutable; generated MCB/GLB seeds and fuzzer working copies live in CI build
directories. The local Clang 19 smoke verification passed for all four input
classes; the existing GLB import CTest also passed.

`SYS-W11-05` is complete. The `release` install component exports
`MeshCraft::Mc3` and `MeshCraft::Mcb`, checks them through an external
`find_package` consumer CTest, and `meshcraft_cli_release` produces a
relocatable CLI `.tar.gz` from the same install rules. The archive includes
the two CLI tools, MC3/MCB headers and libraries, package configs, and the
non-system runtime libraries used by the glTF exporter.

`SYS-W11-06` is in progress. Its Windows CI job remains CNA-independent,
builds and CTests all four standalone components, verifies a fixed
MC3→MCB/GLB fixture before artifact upload, and publishes only the qualified
Windows CLI executables. The release-readiness changes are pushed to
`origin/develop`. This session reconfigured the local MinGW 14 tree with
`BUILD_TESTING=ON`,
confirmed its CTest registration uses the configured cross-test emulator, and
compiled the affected standalone targets. The sandbox blocks Wine itself with
`SIGSYS`, so it cannot provide runtime evidence. The native Linux
`cli_cross_platform_fixture`, package-consumer, capability-documentation, and
plan-consistency CTests passed. Freshly relinked XML, JSON, and MCB
load-policy tests plus the embedded-GLB and SVG safety export tests also
passed. The four path-confinement comparisons use `generic_string()` rather
than Windows-wide `path::native()` strings. Await the first GitHub
`windows-2022` CTest/artifact run before marking this task done.

## Practical commands

```bash
python3 test/validate_plan_consistency.py .
cmake -S . -B b-release -DBUILD_TESTING=ON
cmake --build b-release -j4 --target MeshCraft
ctest --test-dir b-release -LE render --output-on-failure -j4
```

Rendering tests depend on a usable display preflight; their registration or
availability is environment-dependent. Do not write a volatile “all N/N tests
pass” count into handoff documentation unless it was just verified in the same
build tree.

Standalone format/tooling builds are useful when CNA siblings are unavailable:

```bash
cmake -S mc3 -B mc3/build -DBUILD_TESTING=ON
cmake --build mc3/build -j4
ctest --test-dir mc3/build --output-on-failure -j4
cmake -S mcb -B mcb/build -DBUILD_TESTING=ON
cmake --build mcb/build -j4
ctest --test-dir mcb/build --output-on-failure -j4
```

Package and CLI-release verification from the established root build is:

```bash
ctest --test-dir cmake-build-debug -R '^package_consumer_smoke$' --output-on-failure
cmake --build cmake-build-debug --target meshcraft_cli_release -j2
```

## Guardrails

- Preserve `Mc3Document` as the CNA-free canonical AST. Check every consumer
  before changing its public API.
- Keep independent editor/exporter geometry paths independent except for
  deliberate, documented sharing.
- Use four jobs or fewer for local builds/tests.
- Do not modify sibling repositories or push changes without explicit user
  authorization.
- Keep `plan.md` bounded: archive completed narratives instead of letting the
  active queue become a worklog.

## Verification caveat

This session configured the existing root Debug tree and freshly built the
focused standalone/path-confinement targets with `CCACHE_DISABLE=1`; its
configured sibling CNA and sharp-runtime revisions differ from the historical
verified SHAs and must not be treated as a full-editor qualification. The
normal compiler-cache location is read-only in this environment, and Wine is
blocked by sandbox `SIGSYS`, so neither a fresh full suite nor native Windows
runtime tests were completed here. That is not evidence of a source failure.
The next session should start by reviewing the first `standalone-windows`
GitHub Actions run for the current `develop` head; use its CTest/artifact
result to complete or diagnose `SYS-W11-06`.
