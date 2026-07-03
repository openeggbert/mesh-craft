# NEXT.md

_Last updated: 2026-07-03 (later session)_

---

## 1. Project summary

**MeshCraft** is a C++23 3D scene editor for the `.mc3.xml` scene format.
It uses Dear ImGui for its UI, SDL3 + OpenGL via the **CNA** runtime (a
separate sibling repo at `../cna` — do NOT modify from this repo), and
exports scenes to glTF/GLB via **mc3togltf**.

**Main goal:** reach a fully stabilized, test-covered codebase before
adding new features. All work is tracked in `plan.md` as `STAB-XXXX`
tasks across sections S0–S20, gated by a Gate 0–6 checklist (each gate
requires a specific `STAB-XXXX` ID range to be fully green).

**Current phase:** Stabilization. For every gate below, "priority-list
items done" means `plan.md`'s `Priority Execution Order` shortlist for
that gate is closed — **not** the same as the gate being fully green,
which requires its *entire* `STAB-XXXX` range (see `STABILIZATION.md`
for the exact ranges). None of the 7 gates are fully green yet.

- **Gates 0–2** (Build, Format, Export): priority items complete.
- **Gate 3** (Editor safety): P1 items done across S7 (Editor Save/Load),
  S8 (UI Robustness), S13 (Commands/Undo/Redo). P2/P3 remain.
- **Gate 4** (Registry/AI): priority-list items done — S9 (ModelRegistry
  edge cases) and S10 (AI mock tests, the 3 P0 verification items, XSD
  validation of AI responses). P2/P3 remain in both sections.
- **Gate 5** (Large scene): priority-list items done — S6's mesh-reuse
  check (STAB-0243) and 500-object test (STAB-0244). P2/P3 remain
  (including the P3 1000-object stress test, STAB-0245).
- **Gate 6** (Documentation): very close to fully green. S17 is 20/20
  (done). S18's P0/P1 is 7/7 done (18 P2/P3 remain). S19's P0/P1+bonus
  is 11/15 done (4 P2/P3 remain — one, STAB-0631, may already be
  covered by STAB-0383, needs checking). S20 is 12/15 done — the only
  3 remaining items are things this session genuinely could not do:
  STAB-0642 (needs Blender), STAB-0643 (needs a browser), STAB-0650
  (CI is deactivated, can't verify its output without the repo owner
  rotating the PAT first — see §4). Gate 6 needs the full
  `STAB-0576–0650` range green, so it's not there yet, but the
  remaining gap is now small and specific: 18+4+3 = 25 items, 3 of
  which need something outside this session's reach.

Plan-wide totals (out of 650 `STAB-XXXX` rows): **206 ✅ done, 3 🟡
partial, 136 🧪 has a plan but not executed, 305 📋 not started.**

**Important architectural decisions:**
- `mc3/` and `mcb/` are pure C++ static libs with **no** CNA/ImGui
  dependency and must stay independently buildable.
- `mc3togltf/` and `mc3tomcb/` are standalone CLI + lib targets, also
  CNA-free.
- CNA is added as a sibling CMake subdirectory
  (`add_subdirectory(../cna ...)`) and must not be modified from this repo.
- The editor (`src/MeshCraft/`) is CNA-coupled, but many of its command/
  parsing functions turn out to have zero actual CNA dependency once
  ImGui rendering and app-state bookkeeping are set aside. Where that's
  true, the pure logic lives in a dedicated CNA-free header (suffixed
  `Alg` for individual functions) that both the real app code and the
  test suite include and call — see §6.

---

## 2. Current status

### Build
- **Debug** (`cmake-build-debug/`, generated with CLion's bundled cmake
  4.2.2): a full from-scratch rebuild (53 targets) succeeded cleanly,
  last verified 2026-07-02 at commit `f37c541`.
- **Release** (`b-release/`): last verified 2026-07-01. Not re-verified
  since — several commits have landed since then; re-verify before
  relying on it.
- **Offline mode** (`-DFETCHCONTENT_UPDATES_DISCONNECTED=ON`): working as
  of 2026-07-01, not re-verified since.
- **Standalone (CNA-free) component builds**, each configuring/building/
  testing without the root project: `mc3`, `mcb`, `mc3togltf`, `mc3tomcb`.
  Last verified 2026-06-30.

### Tests
**20/20 CTest pass** (last full run 2026-07-03, commit `ee4dd2c`, after a
full reconfigure — see §3's mc3.xsd note):
`smoke_test`, `xsd_validation`, `mc3_registry`, `mc3_ai`, `mc3_roundtrip`,
`mc3_commands`, `mcb_roundtrip`, `mc3tomcb_roundtrip`, `mc3togltf_gltf`,
`mc3togltf_all_primitives`, `mc3togltf_export_verification`,
`mc3togltf_large_scene`, `mc3togltf_csg_strict`, `mc3togltf_csg_export`,
`mc3togltf_csg_unsupported`, `mc3togltf_csg_nested`,
`mc3togltf_instance_deform_cache`, `mc3togltf_float_cache_key`,
`mc3togltf_large_scene_generated`, `mc3togltf_large_scene_500`.

- `mc3_commands` (~250 assertions): editor command algorithms (rename/
  find-replace/array-dup/duplicate/group/ungroup), undo/redo round-trips
  for every mutating command, auto-save/backup, Save-As/Export-Selection/
  drag-drop/invalid-file-load workflows, keybinding/preferences/macro
  persistence formats, hierarchy-panel filtering, material-color
  resolution, undo-stack depth capping, AI-panel + unsaved-changes-
  confirmation dialog lifecycles.
- `mc3_registry`: open/save/search/remove/migration, registry-unavailable
  and search-field edge cases, schema introspection.
- `mc3_ai` (45 assertions): `AiAssistant`'s JSON helpers, the AI-response
  validation pipeline (extract/repair/parse/empty-check/XSD-validate),
  three end-to-end mock-HTTP-server round-trips (success, truncation,
  HTTP error) — no real network call — and XSD accept/reject cases
  (STAB-0391, this session).

See `plan.md`'s `STAB-XXXX` rows for the exhaustive per-behavior list;
this summary intentionally stays high-level.

### Tools / libraries available
- `MeshCraft` — editor executable, version `0.1.0` (`--version` to
  print it). Builds and runs; the 3D viewport is not fully integrated
  into the render loop (see "What does NOT work yet").
- `mc3togltf` — CLI: `mc3togltf in.mc3.xml out.glb`.
- `mc3tomcb` — bidirectional CLI: `mc3.xml` ↔ `.mcb`, direction chosen by
  file extension.
- `Mc3` — static lib, scene data + XML load/save.
- `Mcb` — static lib, binary serialization (`McbWriter`/`McbReader`).
- `mc3togltf_lib` — static lib, `GltfExporter`/`MeshBuilder`/`CsgEvaluator`.

### What works
- Full XML round-trip for all 10 primitive types, including edge cases.
- MCB binary round-trip for the base scene and all N1–N7 extension types.
- MCB CLI round-trip (`mc3.xml → mcb → mc3.xml`), deterministic and
  lossless.
- XSD validation for all test fixtures, including N3–N7.
- glTF/GLB export: all 12 primitives, animations, CSG (including nested),
  materials, lights, cameras, instances, groups, OBJ mesh import.
- SQLite-backed asset registry: open/save/search/remove/migration; search
  matches group/name/tags/description/source, case-insensitively.
- Editor undo/redo: snapshot-based, round-trips full document state for
  every mutating command (including a recently-fixed gap: `resetPivot()`
  now correctly pushes an undo entry).
- Auto-save (configurable interval, separate `.autosave` file) and backup
  rotation (fixed 2-slot ring buffer) on save.
- AI Assistant: sends scene + prompt to the Claude API, extracts/repairs/
  validates the returned XML (including responses wrapped in markdown
  fences or trailing prose — a real bug, fixed in an earlier session
  this same day), validates it against `mc3.xsd` via libxml2 (new this
  session, STAB-0391 — schema-embedded at compile time, no runtime file
  path), applies it to the scene or saves definitions to the registry.

### What does NOT work yet
- `EditorViewport` is not integrated into the `MeshCraftApplication`
  render loop.
- SVG texture rasterization is parsed/serialized but not rasterized
  (stub only) — blocked on a library choice (librsvg vs. NanoSVG).
- Embedded glTF is parsed/serialized but not resolved/inlined by
  `GltfExporter`.
- CI workflow exists but is parked deactivated under `.github_/` (see
  §4); there is no automated full-editor (CNA + SDL3) build/test job
  running anywhere.

---

## 3. Recent changes

All changes are committed on `develop`; see §10 for the exact commit
and push-sync state.

**`mc3.xsd` audit** (not tied to a `STAB-XXXX` ID — this closes out the
"suspected, needs verification" item from an earlier version of this
section): systematically diffed every `SetAttribute`/`NewElement` call
in `Mc3XmlWriter.cpp` against `mc3.xsd`'s declared attributes/elements,
following up on the object `id` gap found while building STAB-0391.
Found **7 more real gaps** — all genuinely round-tripping data (writer
writes it, parser reads it back correctly) that the schema simply never
declared, meaning any scene using them would fail strict XSD validation
(including through the AI response pipeline, since STAB-0391 wired real
validation into it):
- `objectAttrs`: `layer` was completely undeclared.
- Per-object `<state id="...">` children (`Mc3Object::states` — distinct
  from the document-level N6 `<states><state name="...">`) had **zero**
  XSD representation for any object type. Added a new `objectStateType`
  and wired it into all 12 leaf object types + `instanceType` +
  `areaType` + `extrudeType` + `objectsContainerType`.
- `instanceType`: `material_override` and `variants` were undeclared.
- `uvMappingType`: declared a stale, **never-actually-used** combined
  `scale`/`offset` (vec2) pair; the real attributes
  (`projection`/`scale_u`/`scale_v`/`offset_u`/`offset_v`) were entirely
  undeclared.
- `environmentType`: `background_texture`/`skybox_texture` are written
  as separate child elements, but the schema only declared an unused
  `texture` attribute on `<background>` instead.
- `textureElementType`: `name` (display name, independent of `id`) was
  undeclared.

**Also found a genuine, separate round-trip bug** while auditing the
texture `name` gap: the editor's rename-texture UI
(`MeshCraftApplication_UiLeftPanel.cpp:532`) sets `Mc3Texture::name`
independently of the map key/XML `id`; the writer conditionally emits
it as a `name` attribute — but `Mc3XmlParser::parseTextures` always
reset `tex.name = id` on load, **silently discarding the rename on
every save/reload**. This wasn't an XSD problem, it was a real data-loss
bug. Fixed to read `name` back when present (falls back to `id` when
absent, unchanged behavior for files that never used it).

New test coverage: `test/xsd_gaps_fixed.mc3.xml` (new XSD validation
fixture exercising every construct above at once — picked up
automatically by the `xsd_validation` ctest's glob),
`testTextureNameDiffersFromId()` in `roundtrip_test.cpp` (regression for
the parser bug — a positive case confirming a renamed texture survives,
and a negative control confirming unrenamed textures still default
`name` to `id`), and
`testValidateAndParseAiResponseAcceptsPreviouslyUndeclaredConstructs()`
in `ai_test.cpp` (regression for the false-XSD-reject class of bug,
through the exact `validateAndParseAiResponseAlg` pipeline the AI panel
calls).

**Process note, worth remembering**: after editing `mc3.xsd`, a plain
`ninja` rebuild is **not enough** — `mc3.xsd` is embedded into a
generated header (`Mc3XsdEmbed.hpp`) via `configure_file()`, which only
runs at CMake **configure** time, not build time. Editing the schema and
only rebuilding silently compiles in the *old* schema content. Caught
this the hard way: my first attempt at the new regression test failed
even though the schema fix and the test XML were both correct in
isolation — a full `cmake -S . -B cmake-build-debug` reconfigure fixed
it. Always reconfigure after touching `mc3.xsd`.

Verified: root 20/20, standalone `mc3` 1/1 (reconfigured + rebuilt from
scratch to confirm, given the note above).

**S20 (Release Readiness) — STAB-0636/0637/0639/0640/0641/0644-0649**
(12 of 15 items; the other 3 need something this session doesn't have
— Blender, a browser, or an active CI run):
- STAB-0636/0637: `project(MeshCraft VERSION 0.1.0 ...)` +
  `MESHCRAFT_VERSION` compile definition + a new `--version` CLI flag.
  Verified against the real binary: `./MeshCraft --version` →
  `MeshCraft 0.1.0`.
- STAB-0639: **actually built** a fresh Release binary and compared it
  to Debug rather than assuming — 6.25MB vs. 59.9MB (~10× smaller),
  `file`/`objdump` confirm Debug has DWARF debug info and Release has
  none. Release 20/20 ctest also re-verified in the same pass.
- STAB-0640/0641: `xsd_validation` already covers this continuously;
  additionally ran the real `MeshCraft` binary headlessly against
  `house`/`garden_house`/`features.mc3.xml` directly (not just the
  standalone parser) to confirm all three load cleanly through the
  actual editor.
- STAB-0638: new `CHANGELOG.md`. The row's own verification text asks
  for stale pre-replan terminology ("feature groups A–N, stabilization
  groups S1–S12") that doesn't map onto anything in `git log` (which
  shows lettered groups E/F/G/H/M/N/R/S/T, not A–D) — wrote it around
  what's actually there instead, with an explicit note on why.
- STAB-0644/0645: new `THIRD_PARTY.md` — every dependency version
  verified against the real `FetchContent_Declare` `GIT_TAG`, not
  copied from memory.
- STAB-0646: README's "Current Limitations" mostly covered this, but
  **found a real gap** — SVG rasterization wasn't mentioned in README
  at all despite being a known limitation elsewhere; added it plus two
  similarly-missing items (Embedded glTF, N3-N7 runtime execution).
- STAB-0647: new "Reporting a Crash" section — no custom crash handler
  exists (grep-confirmed); Linux instructions actually tested (a
  throwaway SIGSEGV program confirmed the documented `core.<pid>`
  naming); Windows guidance marked explicitly unverified.
- STAB-0648: new `docs/USER_GUIDE.md` — every menu path/shortcut
  verified against the real menu code before writing it.
- STAB-0649: new "Backup and Recovery" section, written from reading
  `MeshCraftApplication_FileOps.cpp` directly (exact autosave path,
  2-slot backup rotation, the real "autosave found" notification
  behavior — a notification only, not an automatic restore prompt).

Verified: root 20/20 and Release 20/20 (both freshly rebuilt after the
`CMakeLists.txt`/`main.cpp` changes for STAB-0636/0637).

**S18 + S19 P0/P1 verification — STAB-0596..0602, 0621..0634** (no
production code changed — pure verification, including live testing
against the real `Mc3` library via throwaway standalone programs, not
just code reading):
- **S18 (Code Quality)**, all 7 P0/P1 items: no editor/CNA code in
  `Mc3`'s actual library source (the literal `grep -r "ImGui" mc3/`
  from the plan's own verification text picks up 4 false positives —
  comments in `mc3/test/*.cpp` *documenting* CNA/ImGui-freedom, not
  real usage; scoped to `mc3/src`+`mc3/include` confirms 0 real hits —
  same pattern for the `EditorAlgorithms.hpp` CNA-free check). No UI
  dependency in `mc3togltf_lib`. Audited `MeshCraftApplication_UiProperties.cpp`
  (58-line data-marshaling shim, no real risk) and the actual
  property-editing logic in `PropertiesPanel.cpp` (~1760 lines) for
  null-pointer risk: the entire body is gated by one
  `if (ctx.selection.hasSelection())`, and every `std::optional`/
  `std::map::find()` access I checked was properly guarded. Same for
  `SceneRenderer.cpp`'s material/mesh pointer accesses — every
  `.find()`/`.value()`/cache-lookup call site checked, all guarded. No
  unguarded null-pointer risk found in either file.
- **S19 (Security)**, all 9 P0/P1 items plus 2 bonus P2s already
  covered by the same checks: API key confirmed never written to disk
  (grepped every persisted-state writer: prefs, autosave, keybindings,
  macro, recent-files — 0 hits) or logged (0 hits for
  `cout`/`cerr`/`printf` near `apiKey` in `AiAssistant.cpp`). No
  `std::system()`/`popen` anywhere in the codebase. SQLite registry
  queries fully parameterized (`sqlite3_bind_*` for every field, no
  string-concatenated SQL). **Actually tested** three DoS-shaped
  inputs against the real parser rather than just reading the code:
  `<include file="../../../../etc/passwd"/>` → clean XML-parse-failure
  error (no crash, no content leak, ~0s); a 10MB file of `<a` repeated
  5M times → throws in ~7ms (no hang); a 10MB-length texture `uri`
  attribute → parses correctly in ~53ms with the full length intact
  (`std::string` has no fixed buffer to overflow). Registry DB path
  confirmed non-injectable — both real call sites use the fixed
  `ModelRegistry::defaultPath()`, no UI input exists for it. The
  include allowed-root policy ask was already satisfied by this
  session's earlier `MC3_FORMAT.md` Include section.
- S20 (Release Readiness) is P2/P3-only — untouched, nothing to verify
  at P0/P1.

Verified: no rebuild needed (no production code changed); the DoS
tests above ran against the already-built `libMc3.a` via small
standalone throwaway `.cpp` files compiled and run directly, not added
to the test suite (they were exploratory verification runs, not new
permanent tests — a future session could promote the more interesting
ones, e.g. the malformed-XML timing test, into `roundtrip_test.cpp` if
desired).

**S17 completed — STAB-0593/0594/0595** (pure docs, no code changes):
closes out the last 3 items in "Documentation and User-Facing Honesty",
all P2/P3.
- STAB-0594: `MC3_FORMAT.md`'s new "Include" section was written by
  reading `mergeInclude()`/`processIncludes()` in `Mc3XmlParser.cpp`
  directly (not from memory or `m1m2m3.md`) — covers what actually gets
  merged (only `<definitions>`/`<materials>`/`<textures>`; an included
  file's `<objects>` etc. are silently ignored), merge order + local-
  override policy, path resolution (relative to the *including* file),
  nested-include depth-first order, diamond dedup, cycle detection, and
  the save-time skip-set mechanism.
- STAB-0593: new `CONTRIBUTING.md` — build setup gotchas (both
  `GLOB_RECURSE` and `mc3.xsd` need a reconfigure, not just a rebuild;
  `cmake-build-debug/` needs CLion's cmake specifically), the CNA
  boundary, and an API change policy that cites this session's
  mc3.xsd/writer symmetry audit as a concrete "why this matters"
  example.
- STAB-0595: new `RELEASE.md` with the 4 requested sections (build
  clean, all tests pass, docs current, sample files valid) plus a
  pre-tag checklist — framed explicitly against the current
  stabilization-phase reality, not implying a release is imminent.

**Gate 6's remaining P1 items** (S17, P1 — pure docs, no code changes):
STAB-0585, 0588, 0589, 0590, 0591 — closes out Gate 6's entire P1
priority-list. Each claim below was verified against actual code before
writing it, not assumed:
- STAB-0585: `MC3_FORMAT.md`'s export support matrix — added 3 rows
  found by tracing the exporter: SVG textures (❌, `svgTextures` is a
  separate map `GltfExporter` never reads — silently dropped, not even
  a warning), Embedded glTF (❌, `embed:id` is treated as a literal OBJ
  path; traced `loadObjMesh`→`buildMesh`'s catch block — it fails,
  prints a warning, and the node exports with `mesh=-1`, i.e. no
  geometry but otherwise valid), N3-N7 (❌, no glTF equivalent).
- STAB-0589: rewrote the CSG limitations paragraph after reading
  `CsgEvaluator.cpp`'s `manifoldToMeshData()` directly — corrected "no
  UVs" to the more precise fact that a texcoord channel *is* written
  but every value is a hardcoded `(0,0)` placeholder (not simply
  absent); confirmed flat-normal recomputation via the actual
  cross-product code; named "strict mode" as the actual default
  explicitly (`allowApproximateCSG = false`).
- STAB-0590/0591: added an AI + Registry limitations block to README's
  "Current Limitations". Notable verified findings: (a) the AI API key
  is never persisted to disk — `PrefsAlg`'s fields have no `apiKey`
  member; (b) `mc3.xsd` has **zero** `minInclusive`/`minExclusive`
  constraints anywhere (`grep`-confirmed), so a schema-valid AI response
  can still contain e.g. a negative `size` and will apply to the scene
  unchanged; (c) also found and fixed an unrelated **stale claim** in
  the same README section — it said "only auto-save interval is
  persisted; snap/grid/theme not saved", but `savePrefsAlg`/
  `loadPrefsAlg` (read directly) persist all 6 `PrefsAlg` fields
  (autoSaveInterval, snapTranslate, snapRotate, snapScale, gridSpacing,
  theme). Fixed in the same edit since it was directly adjacent and
  already verified false.
- STAB-0588: reviewed `m1m2m3.md` (the M1/M2/M3 design doc) against
  current code. M1 (`<include>` semantics) and M2 (registry schema/C++
  interface) still match closely. Found and fixed real drift in M2
  (search hint missing `source`, added post-doc per STAB-0344) and
  significant drift in M3's `AiAssistant` class snippet/lifecycle
  diagrams — `sendAsync` grew from 2 to 3 params (prompt caching),
  `maxTokens`/`apiBaseUrl`/`wasTruncated()` were all added later, and
  the validation pipeline now includes an mc3.xsd step (STAB-0391)
  the doc never mentioned. Added a "reviewed 2026-07-03" note at the
  top of the file pointing at what changed.
- Verified: touched no `.cpp`/`CMakeLists.txt` files — pure docs, no
  rebuild/retest needed (root ctest last verified 20/20 at `d63bb15`).

**Gate 6 priority-list documentation cluster** (S17, P1/P2 — pure docs,
no code changes): STAB-0579, 0580, 0581, 0583, 0584, 0586, 0587, 0592.
- STAB-0580/0581/0587 were already satisfied by earlier work — just
  verified and marked ✅ in `plan.md` (no file changes for these three).
- STAB-0579: `README.md` — added a note that `file(GLOB_RECURSE)`
  requires a cmake reconfigure (not just `ninja`) after adding a new
  `.cpp` file.
- STAB-0583: `MC3_FORMAT.md` — added `Meta (N7)`, `Scripts (N3)`,
  `Sounds and Music (N4)`, `Triggers (N5)`, `Scene States (N6)`
  sections, each with a real example drawn from `test/n*_*.mc3.xml`
  fixtures, an attribute table, and an honest status note (all five are
  fully round-tripped — parser/writer/MCB/XSD — but **not executed at
  runtime**: no Lua interpreter, no audio playback, no trigger-firing
  event system, no state-switching logic). Also updated the "Top-level
  sections" overview and added a note that the XSD enforces a strict
  root-element order — newly relevant since STAB-0391 wired real XSD
  validation into the AI response pipeline.
- STAB-0584: `MC3_FORMAT.md` — added an `MCB Binary Format` section
  (what it is, file extension, how to produce it via CLI/C++ API,
  header byte layout, payload tag encoding, relationship to
  `.mc3.xml`), verified against `McbFormat.hpp`/`McbWriter.hpp`/
  `McbReader.hpp` rather than guessed.
- STAB-0586: `STABILIZATION.md` — full rewrite. The old version
  described "15 CTest tests" and listed gaps that are now fixed (MCB
  roundtrip test, N3-N7 XSD fixtures, AI mock tests — all exist now).
  Replaced with an accurate gate table (exact `STAB-XXXX` ranges +
  priority-list status per gate), the current 20-test list, and a
  "Known Gaps" section that points to this file's §5 as the actively-
  maintained source of truth instead of duplicating a list that will
  go stale again.
- STAB-0592: new `TESTING.md` — how to run tests (full suite, single
  test, direct binary, standalone builds), a reference table for all
  20 tests with purpose + pass criteria (assertion counts for the 5
  C++ binaries were measured by actually running them, not guessed),
  and a "writing a new test" section.
- Verified: this cluster touched no `CMakeLists.txt`/`.cpp` files —
  `git status` after the change showed only `.md` files — so no
  rebuild/retest was performed for it specifically (root ctest was
  last verified 20/20 by the STAB-0243/0244 work just before this).

**STAB-0377/0378/0379** (S10, P0 — verification-only, no new code):
confirmed all three were already satisfied by existing code, so each was
simply marked ✅ in `plan.md` with a citation, no new test/code added:
- STAB-0377: `testUndoRedoAiApply()` (`mc3/test/editor_commands_test.cpp:974`)
  already covers "Apply to Scene" pushing an undo entry; verified it
  matches the real button code exactly (`MeshCraftApplication_UiAi.cpp:264-266`,
  `pushUndo(); document_ = *aiPendingDoc_;`).
- STAB-0378: `grep` over `AiAssistant.cpp` confirms the API key is only
  ever used to build the `x-api-key` HTTP header — never written to
  `std::cout`/`std::cerr`/`printf`/etc.
- STAB-0379: `MeshCraftApplication_UiAi.cpp:138-143` already pre-fills
  the key input buffer from `std::getenv("ANTHROPIC_API_KEY")` whenever
  the buffer is empty.

**STAB-0391** (S10, P1 — real feature work, new dependency): added real
XSD validation of AI-generated XML, closing the last item in Gate 4's
`Priority Execution Order` list.
- New `validateXmlAgainstXsdAlg(xml)` in `AiResponseAlgorithms.hpp`
  (libxml2 `xmlSchema*` API), wired into `validateAndParseAiResponseAlg`
  right after the structural-parse + empty-document checks.
- `mc3.xsd` is embedded into a generated header at CMake configure time
  (new `cmake/Mc3XsdEmbed.hpp.in` → `generated/MeshCraft/Mc3XsdEmbed.hpp`,
  via `file(READ)` + `configure_file(... @ONLY)` in root `CMakeLists.txt`)
  — validation never depends on a runtime file path or install layout.
- New optional `find_package(LibXml2)` (desktop builds only), same
  found/not-found pattern as SQLite3/OpenSSL: sets `MESHCRAFT_HAS_LIBXML2`
  and links `libxml2` into both `MeshCraft` and `ai_test` when present;
  when absent, `validateXmlAgainstXsdAlg` is a no-op that always reports
  "valid" — schema validation degrades gracefully rather than blocking
  "Apply to Scene" on a build without libxml2 (tinyxml2 structural
  parsing already happened upstream either way). `libxml2-dev` 2.9.14
  was present on this dev machine and is now a linked dependency of the
  `MeshCraft` binary — confirmed via `ldd MeshCraft | grep xml`.
- **Found and fixed a real, pre-existing schema/format gap** while
  building this: `Mc3XmlWriter.cpp` writes an `id` attribute on every
  scene object and `Mc3XmlParser.cpp` reads it back, but `mc3.xsd`'s
  `objectAttrs` group never declared `id` as a valid attribute — so any
  object carrying `id` (which is all of them, in practice) would have
  failed strict XSD validation, even though it's a real, actively-used
  part of the format. Fixed by adding
  `<xs:attribute name="id" type="xs:string"/>` to `objectAttrs`.
  Deliberately typed `xs:string`, not `xs:ID`: object ids aren't
  referenced via IDREF anywhere, and `xs:ID` would pull them into the
  same document-wide uniqueness namespace as material/texture/definition
  ids — a constraint the app doesn't actually enforce.
- 6 new assertions in `ai_test.cpp`: accept a schema-valid document;
  reject `role="bogus"` (`roleType` only allows `"cutter"`) both
  directly via `validateXmlAgainstXsdAlg` and through the full
  `validateAndParseAiResponseAlg` pipeline.
- Verified: root 19/19 (at the time, before STAB-0243/0244 below added a
  20th test; including `xsd_validation`, confirming the schema fix
  didn't break any existing fixture), standalone `mc3` 1/1, `MeshCraft`
  full rebuild links clean against libxml2.

**STAB-0243/0244** (S6, P1/P2 — Gate 5's `Priority Execution Order`
items): STAB-0243 was already fully covered by the existing
`large_scene_generated_test.py` (its assertions check `len(meshes) <= 6`
and `nodes >= meshes * 10`, i.e. reuse, not "200 unique meshes") — no new
test needed, just a `plan.md` status update. STAB-0244 parameterized
that same script with an optional CLI scale-factor arg instead of
duplicating it (`N_INSTANCES/N_SPHERES/N_BOXES = round(100/50/50 *
SCALE)`), added `time.monotonic()` timing with an explicit `assert
elapsed < 30.0`, and registered a new `mc3togltf_large_scene_500` ctest
(`mc3togltf/CMakeLists.txt`, scale `2.5` → 500 objects). Measured:
500-object export completes in ~0.02s. Root ctest is now **20/20**;
standalone `mc3togltf` build re-verified separately (12/12, includes the
new test).

Across this session, Gate 3's P1 items (S7/S8/S13) and all of Gate 4
(S9 + S10) were closed out, cluster by cluster, each verified with a
full Debug rebuild + `ctest`. **5 real bugs were found and fixed** along
the way (not just test-coverage additions):

1. **`resetPivot()` never pushed an undo entry** — it mutated selected
   objects' transform and set `modified_ = true`, but didn't call
   `pushUndo()`. Fixed in `MeshCraftApplication_Commands.cpp`.
2. **`SceneHierarchyPanel.cpp`'s hierarchy filter did nothing when only a
   type/layer/tag/material filter was set (no search text)** — the skip
   decision was gated on the text-search flag alone instead of "any
   filter active". Fixed in `drawHierarchy()`.
3. **Bloom/SSAO strength (and SSAO radius) sliders had no
   `ImGuiSliderFlags_AlwaysClamp`** — Ctrl+Click-to-type could set them
   to out-of-range values, including negative, with nothing else
   guarding the shader uniforms. Fixed in `MeshCraftApplication_UiMenuBar.cpp`.
4. **`ModelRegistry::search()`'s SQL omitted `source` from its `WHERE`
   clause** — searching by source silently found nothing. Fixed, plus
   the search hint text was updated.
5. **AI responses wrapped in a markdown code fence or followed by
   trailing prose failed to parse** — `extractXml()` only located the
   *start* of the XML and returned everything to the end of the string;
   tinyxml2 does not tolerate content after the root element closes.
   Fixed by also trimming at the matching `</mc3>`.

Also this session: extended test coverage across S7/S8/S9/S10/S13
(dozens of `STAB-XXXX` items — see `plan.md` for the full list), added
CNA-free "Alg mirror" functions for previously-untestable logic
(hierarchy filtering, material-color resolution, undo-stack depth
capping, keybinding/prefs/macro persistence formats, AI-response
validation), added a test seam (`AiAssistant::apiBaseUrl`) enabling a
real mock-HTTP-server test without touching production behavior, and
created new files `src/MeshCraft/AiResponseAlgorithms.hpp` and
`mc3/test/ai_test.cpp`.

Also found and corrected two stale/outdated docs: `AI_TRUNCATION_BUG.md`
describes a bug (hardcoded `max_tokens=8192`, no `stop_reason` check)
that isn't present in the current code — it was fixed in an earlier
session but the doc was never updated. `plan.md`'s per-section
task-count summary table had also drifted from actual row state over
several prior sessions; it was recomputed directly from the row markers
and is now a derived table, not hand-maintained.

Earlier sessions (summarized): the original Gate 3 "commands are
undoable" cluster, auto-save + 2-slot backup rotation, AI-panel dialog
lifecycle, dirty-flag/unsaved-changes-confirmation, registry search
fields + edge cases, plus 2 more real bugs (7 of 40 `std::strncpy` call
sites missing their null-terminator; the `ModelRegistry::search()` gap
listed above).

---

## 4. Current blocker / main problem

**There is no blocker to local development or testing.** Debug builds
clean (full rebuild, 53/53 targets) and 20/20 tests pass as of the last
verified state (2026-07-03; see §10 for the exact commit and push-sync
status). `develop` may be ahead of `origin/develop` — check before
assuming a push is current.

The one open **operational** issue is **CI cannot be activated with the
current git credentials**:

- Symptom / failing command (only when trying to activate CI, not for
  normal pushes):
  ```
  git push origin develop
  ! [remote rejected] develop -> develop (refusing to allow a Personal
    Access Token to create or update workflow `.github/workflows/ci.yml`
    without `workflow` scope)
  ```
- Cause: the PAT in `.git/config`'s remote URL (both this repo and the
  sibling `../cna` repo) lacks the `workflow` OAuth scope.
- Workaround already in place: the CI workflow file is committed but
  parked at `.github_/workflows/ci.yml` (GitHub only treats
  `.github/workflows/` as live workflows), so it's versioned but
  inactive. To activate: rename `.github_` → `.github` and push with a
  `workflow`-scoped token.
- Related, separate issue: that same PAT is embedded in plaintext in the
  remote URL in `.git/config` (not in any tracked/committed file — full
  history checked). Recommended fix: revoke it, issue a new token with
  `repo` + `workflow` scopes, and switch the remote to a credential
  helper or SSH instead of embedding the token in the URL.

Nothing has been tried yet to fix this beyond identifying it; it
requires the repo owner to rotate/rescope the token.

---

## 5. Known bugs and limitations

- **PAT exposed in `.git/config` and lacks `workflow` scope** — see §4.
  _status: confirmed (security + operational); needs owner action._
- **CI is partial** — only the CNA-free libs (`mc3`, `mcb`, `mc3togltf`,
  `mc3tomcb`) are covered by the parked workflow; there's no full-editor
  (CNA + SDL3) CI job. _status: incomplete, and currently inactive
  (see §4)._
- **SVG texture rasterization** — parsed/serialized only, not rasterized.
  Blocked on a library choice (librsvg vs. NanoSVG). _status: incomplete._
- **Embedded glTF** — parsed/serialized but not inlined by
  `GltfExporter`. _status: incomplete._
- **`EditorViewport` not integrated** into `MeshCraftApplication`'s
  render loop. _status: incomplete._
- **`mc3` standalone build skips `mc3_commands`** — that test needs
  `EditorAlgorithms.hpp` from the editor tree, which isn't present in a
  standalone `mc3/` checkout. _status: intended, not a bug._
- **`cmake-build-debug/` reconfigure is toolchain-sensitive** — it was
  generated with CLion's bundled cmake 4.2.2; reconfiguring with the
  system cmake (3.31.6) fails during Generate (bogus manifold/
  `boolean3.cpp` sources). _status: confirmed, environmental — always
  use CLion's cmake for this directory._
- **`AI_TRUNCATION_BUG.md` is stale** — describes `max_tokens` hardcoded
  at 8192 with no `stop_reason` check; neither is true of the current
  code (`maxTokens` defaults to 32000, UI-configurable up to 64000, and
  `wasTruncated()` is checked and surfaced). The underlying bug was
  fixed in an earlier session; the doc was never updated.
  _status: doc is stale, not the code — consider deleting or rewriting
  `AI_TRUNCATION_BUG.md`._
- **Gate 3 is not fully green** (P1 items are, but the gate needs the
  full `STAB-0261–0335` range) — S7 28/35, S8 17/40, S13 11/25; the
  remainder of each is P2/P3, untested. _status: needs verification,
  tracked task-by-task in `plan.md`._
- **S10 (AI) P0 items are now all closed** (STAB-0377/0378/0379
  verified, no code changes needed) and Gate 4's `Priority Execution
  Order` list is fully done (STAB-0391 XSD validation added); 24 P1/P2/P3
  items remain untouched. _status: priority-list items done, rest not
  started; note Gate 4 itself still needs the full `STAB-0336–0410`
  range green to be considered closed._
- **`mc3.xsd` vs. `Mc3XmlWriter.cpp` audit — done, 7 more gaps found and
  fixed** (see §3): `layer`, per-object `<state>` (all object types),
  `instance`'s `material_override`/`variants`, `uv_mapping`'s real
  attributes, `environment`'s `background_texture`/`skybox_texture`,
  `texture`'s `name`. All confirmed genuinely round-tripping (not just
  schema gaps) before fixing. _status: resolved. Residual risk: the
  audit covered `Mc3XmlWriter.cpp` exhaustively but not every
  `Mc3XmlParser.cpp` code path in the other direction (parser accepting
  something the writer never emits) — lower priority since that
  direction can't cause a false-XSD-reject of real writer output._
- **Texture `name` silently reset to `id` on every save/reload** — found
  and fixed during the audit above (`Mc3XmlParser::parseTextures` never
  read the `name` attribute back). _status: fixed, regression-tested
  (`testTextureNameDiffersFromId`)._
- **`mc3.xsd` has no numeric range constraints at all** — zero
  `minInclusive`/`minExclusive` anywhere in the schema (`grep`-confirmed,
  STAB-0590). A structurally/schema-valid AI response can contain a
  negative `size`, `radius`, `duration`, etc. and it will pass validation
  and apply to the scene unchanged. _status: confirmed, documented in
  README's "Current Limitations" — not fixed; fixing would mean deciding
  per-attribute what ranges are actually valid, a bigger design task than
  a documentation pass._
- **N3-N7 extensions (scripts, sounds, music, triggers, scene states) are
  data-only** — fully round-tripped (XML parser/writer, MCB, XSD), but
  nothing executes them at runtime: no Lua interpreter, no audio
  playback, no trigger-firing event system, no "switch active scene
  state" logic. Documented explicitly in `MC3_FORMAT.md` per-section
  now (STAB-0583, this session). _status: intended at this stage (data
  model before execution), not a bug — but a real capability gap a user
  reading only the format spec's examples could easily miss without the
  new status notes._
- **Gate 5 and Gate 6 are close but not fully green** — Gate 5 needs
  the full `STAB-0411–0470` range (S6 is 7/25 ✅). Gate 6 needs
  `STAB-0576–0650` — S17 20/20 ✅, S18 7/25 (P0/P1 done, 18 P2/P3
  left), S19 11/15 (P0/P1+bonus done, 4 P2/P3 left), S20 12/15 (only 3
  items left, and all 3 are genuinely blocked — see below).
  _status: very close for Gate 6; the remaining 18+4 S18/S19 items are
  normal follow-on work, see §8._
- **3 `plan.md` items cannot be completed by an AI session in this
  environment**: STAB-0642 (needs Blender 4.x installed, not available
  here), STAB-0643 (needs a browser to interactively verify a web
  build), STAB-0650 (needs CI actually running, but CI is parked
  deactivated — see §4's PAT issue; reviewing the YAML's *configuration*
  is a materially weaker check than what the row asks for). _status:
  flagged, not attempted further — these need either a human with the
  right tools, or the PAT rotated first._

---

## 6. Architecture notes

```
MeshCraft (editor exe)
├── CNA (SDL3/OpenGL runtime — sibling repo at ../cna, DO NOT MODIFY)
├── Mc3 (static lib — mc3/): Mc3Document, XML parser/writer, all types
├── Mcb (static lib — mcb/): McbWriter, McbReader, MCB binary format v1
├── mc3togltf_lib (static lib): GltfExporter, MeshBuilder, CsgEvaluator
├── mc3togltf (CLI exe): thin wrapper around mc3togltf_lib
└── mc3tomcb (CLI exe): mc3.xml <-> mcb converter (links Mc3 + Mcb)
```

**Data flow:** `.mc3.xml` → `Mc3XmlParser` → `Mc3Document` (in-memory
model) → either `Mc3XmlWriter` (save), `McbWriter` (binary), or
`GltfExporter` (glTF/GLB).

**Undo/redo:** snapshot-based. `undoStack_`/`redoStack_` (capped at
`kUndoMax = 20`, oldest entries dropped first) hold
`std::vector<Mc3::Mc3Document>`; `pushUndo()` stores a deep copy of
`document_` before each mutating command.

**The "Alg mirror" pattern**, used throughout this codebase's test
suite: editor/AI logic in CNA-coupled `.cpp` files (`MeshCraftApplication_
*.cpp`, `MeshCraftApplication_UiAi.cpp`) often turns out to have zero
actual CNA/ImGui dependency once app-state bookkeeping and rendering
calls are set aside. Two variants, both in use:
- **Single source of truth** (preferred): move the pure logic into a
  CNA-free header (`EditorAlgorithms.hpp`, `AiResponseAlgorithms.hpp`),
  suffix functions `Alg`, and have the real `.cpp` `#include` and call
  them directly — no duplication. Examples: `duplicateObjectsAlg`,
  `groupObjectsAlg`, `extractXmlAlg`, `validateAndParseAiResponseAlg`.
- **Kept in sync manually**: only when the real function is genuinely
  CNA-coupled and can't be unified (e.g. `deepCopyDoc` vs. the mirror
  `deepCopyObjectAlg`) — the mirror's comment names the real function it
  tracks.

Where a class has no CNA dependency at all (`ModelRegistry`,
`Mc3Document`), tests use the real class directly — no mirror needed.
Where logic is genuinely inseparable from ImGui rendering, verification
is by code inspection only (documented as such in `plan.md`), since no
headless ImGui test harness exists.

**Testing network-dependent code**: `AiAssistant` has a public
`apiBaseUrl` member (default `https://api.anthropic.com`) that tests
override to point `sendAsync()` at a local `httplib::Server` mock —
`httplib::Client` auto-dispatches to SSL for `https://` or a plain
socket for `http://`, so this changes nothing about production
behavior. See `mc3/test/ai_test.cpp` for the pattern.

**Embedding a resource file at compile time**: `mc3.xsd` is compiled
into `MeshCraft`/`ai_test` as a raw string constant
(`MeshCraft::kMc3XsdContent`) rather than read from disk at runtime.
Root `CMakeLists.txt` does `file(READ mc3/mc3.xsd MC3_XSD_CONTENT)` then
`configure_file(cmake/Mc3XsdEmbed.hpp.in .../generated/MeshCraft/
Mc3XsdEmbed.hpp @ONLY)`, substituting `@MC3_XSD_CONTENT@` into a
`R"MC3_XSD_EMBED(...)MC3_XSD_EMBED"` raw string literal. This is the
project's first use of this pattern — reach for it again for any other
resource that must be available regardless of CWD/install layout,
instead of a runtime path lookup.

**Hard constraints / invariants:**
- `Mc3Document` public API: do not change without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` must stay buildable
  standalone (no CNA/ImGui deps). Keep editor-only test dependencies
  guarded (see `mc3/CMakeLists.txt`'s check for `EditorAlgorithms.hpp`'s
  presence).
- Do not register tests referencing the `mc3tomcb` target inside `mcb/`
  (would break the `mcb` standalone build) — `mc3tomcb_roundtrip` lives
  in `mc3tomcb/CMakeLists.txt`.
- Do not add `${meta-gl_SOURCE_DIR}/include` to any `CMakeLists.txt`
  (triggers a full CNA recompile).
- Do not modify CNA source files from this repo.
- `file(GLOB_RECURSE)` is used for sources — adding a new `.cpp` file
  requires a cmake reconfigure (a new `.py` test file does not; new test
  executables registered explicitly in `CMakeLists.txt`, like
  `ai_test`, need a reconfigure too).
- `mc3.xsd` changes require a cmake reconfigure too (it's embedded into
  a generated header at configure time, not read at build/run time —
  see §6 "Embedding a resource file at compile time").
- Editing `mc3.xsd`: any object attribute that `Mc3XmlWriter.cpp`
  actually writes must also be declared in the schema (`objectAttrs` or
  the specific element type) or real/AI-generated scenes using it will
  fail XSD validation — see the `id` gap fixed in STAB-0391 (§3) and the
  suspected-wider-gap note in §5.
- MCB format version is `MCB_VERSION = 1` in `McbFormat.hpp` — bump on
  any breaking wire-format change.
- XSD root element order: `include → metadata → meta → environment →
  lights → cameras → textures → materials → embeds → scripts → sounds →
  music → triggers → states → definitions → objects → actions`.
- Reconfigure `cmake-build-debug/` only with CLion's bundled cmake 4.2.2.

---

## 7. Useful commands

```bash
# --- Debug (CLion dir; reconfigure with CLion's cmake to avoid the 3.31.6 bug)
CLION_CMAKE=/home/robertvokac/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake
"$CLION_CMAKE" -S . -B cmake-build-debug -DBUILD_TESTING=ON   # (re)configure
cd cmake-build-debug && ninja && ctest --output-on-failure    # build + test (20)
ctest -N                                                      # lists all 20 tests

# --- Release (system cmake 3.31.6 is fine for a fresh dir)
cmake -S . -B b-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
cmake --build b-release -j4
(cd b-release && ctest --output-on-failure)

# --- Standalone (CNA-free) component builds + tests
for c in mc3 mcb mc3togltf mc3tomcb; do
  cmake -S "$c" -B "$c-build" -G Ninja -DBUILD_TESTING=ON \
        -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
  cmake --build "$c-build" -j4
  (cd "$c-build" && ctest --output-on-failure)
done   # mc3 1/1 · mcb 1/1 · mc3togltf 12/12 · mc3tomcb 2/2

# --- Export a scene / validate XML / run a single test / check version
./cmake-build-debug/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb
python3 test/validate_xsd.py mc3/mc3.xsd test/features.mc3.xml
./cmake-build-debug/MeshCraft --version                      # MeshCraft 0.1.0
ctest -R mc3_commands --output-on-failure   # editor algorithms + undo/redo
ctest -R mc3_registry --output-on-failure   # ModelRegistry
ctest -R mc3_ai       --output-on-failure   # AiAssistant + mock HTTP server
ctest -R mc3tomcb_roundtrip --output-on-failure

# --- Push (normal pushes work fine; only CI activation is blocked, see §4)
git push origin develop
# To activate CI: rename .github_ -> .github and push with a workflow-scoped token
```

No project linter/formatter is configured.

---

## 8. Next smallest tasks

1. **Gate 6 is nearly closed.** S17 20/20, S18's P0/P1 7/7, S19's
   P0/P1+bonus 11/15, S20 12/15 — the only 3 remaining items in the
   whole gate that this session could plausibly still do are S18's 18
   P2/P3 items and S19's 4 P2/P3 items (STAB-0642/0643/0650 are
   genuinely blocked — see §5). Doing all of S18+S19's remaining P2/P3
   would fully close Gate 6's `STAB-0576–0650` range except those 3.
   Goal: STAB-0610 (`-Wall -Wextra` — will surface real warnings across
   the whole codebase, potentially a bigger task than it looks once you
   see the count), STAB-0630 (untrusted OBJ robustness — mirrors the
   DoS-input testing pattern already used for STAB-0625/0628/0629, just
   against `tinyobjloader` instead of the XML parser), STAB-0631 (check
   if `AiAssistant`'s network timeout is already covered by something
   equivalent to STAB-0383 before writing new work), then the rest of
   S18's P2/P3 by ID order.
   Files: varies — see `plan.md`'s Key File column per row.
   Verify: varies; STAB-0610 needs a rebuild to see warning output,
   STAB-0630 needs an actual malformed-OBJ test like the XML DoS tests.

2. **Move to P2 items** in whichever section is most valuable next —
   with P0/P1 essentially exhausted across S6/S7/S8/S9/S10/S13/S18/S19,
   the next tier by `plan.md`'s own priority scheme is P2 (then P3).
   Remaining P2/P3 counts: S6: 13 (includes STAB-0245, the P3
   1000-object stress test), S7: 7, S8: 23, S9: 13, S10: 24, S13: 14,
   plus untouched sections S11 (Materials, 30), S12 (Animation, 30),
   S14 (Rendering, 30), S15 (Import/export, 25).
   Goal: pick by ID order within the chosen section; for each item,
   check whether the relevant logic is CNA-free (an `Alg`-mirror
   candidate) or inspection-only before assuming a CNA/ImGui test
   harness is needed.
   Files: varies by section — see `plan.md`'s Key File column per row.
   Verify: the relevant `ctest -R <target>`, plus a full
   `ctest --output-on-failure` (expect 20/20 or higher).

3. **(optional) Rotate the PAT and activate CI** — see §4 for the exact
   steps.
   Goal: replace the plaintext, under-scoped PAT in `.git/config` with a
   `repo`+`workflow`-scoped token (or SSH), then rename `.github_` →
   `.github`.
   Files: `.git/config` (not tracked), `.github_/workflows/ci.yml` →
   `.github/workflows/ci.yml`.
   Verify: `git push origin develop` accepted for a
   `.github/workflows/` file, and the workflow runs in GitHub Actions.

---

## 9. Do not do yet

- **No new scene-format features** — N1–N7 are complete; further schema
  additions need design discussion first.
- **No CNA source changes** — a separate Claude Code instance owns the
  `../cna` repo.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, and all test XMLs.
- **No `${meta-gl_SOURCE_DIR}/include`** in any `CMakeLists.txt`.
- **No reconfiguring `cmake-build-debug/` with system cmake 3.31.6** —
  use CLion's cmake 4.2.2.
- **No moving `.github_` back to `.github`** until a `workflow`-scoped
  token exists (the push will be rejected).
- **No committing the PAT** anywhere — it must be removed from
  `.git/config`, not copied into any tracked file.
- **No SVG rasterization work** until a library choice is made (librsvg
  vs. NanoSVG).
- **No mass refactoring** of passing code, and no speculative
  architecture changes — this is a stabilization phase; scope each task
  to exactly what its `STAB-XXXX` entry in `plan.md` asks for.
- **No rewriting `AI_TRUNCATION_BUG.md` as a side effect of an unrelated
  task** — it's flagged as stale (§5), but cleaning it up should be its
  own small, deliberate step, not folded into other work.
- **No typing object `id` (or any newly-added `mc3.xsd` attribute) as
  `xs:ID`** without first checking whether the app actually enforces
  document-wide uniqueness for it — `xs:ID` pulls the attribute into a
  shared uniqueness namespace with materials/textures/definitions, which
  is not an invariant the current writer/parser upholds for object ids
  (see STAB-0391 in §3).

---

## 10. Resume prompt

```
Read NEXT.md first. Then inspect only the files needed for the first
task in section 8. Do not refactor unrelated code. Make one small,
verified improvement. Run the relevant build/test command from section 7
and confirm cmake-build-debug still passes (20/20, or the new total if
you registered a new ctest). Update NEXT.md after finishing.

Current branch: develop; confirm sync with origin/develop before
resuming — check `git status` / `git log origin/develop..HEAD` (recent
local commits may not be pushed yet; push only if asked).
Build dirs: cmake-build-debug/ (Debug, CLion cmake 4.2.2) — full
reconfigure + rebuild (53 targets) + 20/20 ctest verified clean
2026-07-03 at commit fca6fc1 (includes the version-number/--version-flag
change, which needed a reconfigure for the new PROJECT_VERSION). Release
(b-release/) also freshly built + 20/20 ctest verified in the same pass
— 6.25MB, no debug symbols, confirmed via `file`/`objdump`. Standalone
mc3/ build re-verified 2026-07-03 (1/1); standalone mc3togltf/ build
re-verified 2026-07-03 (12/12).
Active plan: plan.md (STAB-XXXX tasks; every gate's P1 priority-list
subset is done; S17 is fully complete at 20/20; S18's P0/P1 is 7/7;
S19's P0/P1+bonus is 11/15; S20 is 12/15 with only 3 items left, all
genuinely blocked — STAB-0642 needs Blender, STAB-0643 needs a browser,
STAB-0650 needs CI actually running, which it can't since it's parked
deactivated) — S6 7/25, S7 28/35, S8 17/40, S9 22/35, S10 10/40,
S13 11/25, S17 20/20, S18 7/25, S19 11/15, S20 12/15. Note: no gate is
fully green yet — each needs its full STAB-XXXX range; Gate 6 is close
— only S18's 18 P2/P3 items and S19's 4 P2/P3 items stand between it
and green, besides the 3 blocked S20 items (see plan.md's
"Stabilization Gates" table, or the equivalent table in
STABILIZATION.md). Also this session (not tied to a STAB-XXXX ID): a
full mc3.xsd audit found and fixed 7 more undeclared writer
attributes/elements plus one real texture-rename round-trip bug (see
§3) — closed out. Pick the next task from section 8: S18/S19's
remaining P2/P3 items (would fully close Gate 6 except the 3 blocked
S20 items), move to P2 items in whichever section is preferred, or
optionally rotate the PAT to activate CI).
Reconfigure cmake-build-debug ONLY with CLion's cmake 4.2.2, not
/usr/bin/cmake.
CI is parked deactivated under .github_/ (token lacks `workflow` scope,
see section 4).
```
