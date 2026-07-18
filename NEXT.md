# NEXT.md

_Update, 2026-07-18 (new session, following a fresh independent re-audit):
a from-scratch, four-agent adversarial audit (build/test verification,
core-code bug audit, architecture/docs staleness, UX gap audit — none of
them trusting this file's own "backlog exhausted" claim below at face
value) found the backlog genuinely was accurate on build/test health but
surfaced 12 new, previously-undocumented findings (ranked by severity) plus
several stale docs. The user picked the most severe first: **`AUD-064`**
(commit `ecbe3e7`) — a `<grid subdivisions_x="4096" subdivisions_z="4096"/>`
was individually legal per-field but the PRODUCT was unbounded, rebuilding
~16.8M vertices from scratch every single frame with no cache and freezing
the live editor (empirically confirmed: unpatched binary did not finish a
`--screenshot` within 20s on the stress fixture; patched, ~3s). Fixed with
a bounds check before allocating, matching `drawExtrudeDynamic`'s own
existing fallback convention in the same file. New `test/grid_stress.mc3.xml`
+ `smoke_test_grid_stress` ctest (regression guard). Full rebuild + fresh
root `ctest` immediately after this fix, before the second fix below and
before the blocker described next: 133 of 133 tests passing (was 132).
Second: **`AUD-065`** (commit `2ac7db4`) — `Mc3JsonParser.cpp` (the
`.mc3.json` load path) applied NONE of the per-field tessellation clamps
(`kMaxTessellation=4096`) the XML path has enforced since `AUD-005`, so a
hand-edited or AI-generated `.mc3.json` bypassed that hardening entirely,
including `AUD-064`'s own Grid case. Fixed with a `clampTess()` helper at
all 6 read sites (primitive segments/subdivisionsX/Z, extrude cross-section
sides/segments, extrude path segments); new `mc3_json_input_budget` test
(13 assertions), mirrors `mc3_input_budget`'s XML coverage. Verified via
the standalone CNA-free `mc3/build` tree: 20 of 20 tests passing (was 19).
An external, unrelated `../easy-gl`/`../meta-gl` mid-edit (discovered
while verifying this item) briefly blocked the CNA-linked root build in
between — see §4's now-`RESOLVED` note — so the full root suite couldn't
be re-verified until that cleared; re-checked afterward on the user's
prompt and confirmed clean: full root `ctest -j4`, 134 of 134 tests
passing (`plan_consistency` included), both fixes good end-to-end. Third:
**`AUD-066`** (commit `42c24cb`) — `McbReader.cpp` validated every claimed
collection count against a 10M sanity ceiling (`rU32Bounded`) but then
`.reserve()`'d the FULL claimed count up front at all 13 call sites,
before reading a single element — a tiny corrupted/malicious file (valid
fields up to one oversized-but-legal count, then EOF) could force a large
up-front allocation. Fixed with `reserveHint(n) = min(n, 4096)`. New
`mcb_reserve_bomb` test (11 assertions) measures `/proc/self/status`
VmPeak, not RSS — empirically confirmed `reserve()` alone leaves RSS flat
(Linux lazy page commit) while VmPeak on the unpatched reader jumped from
~6MB to ~850MB for a 43-byte hostile file claiming 9,000,000 lights;
patched, only ~350KB. Verified both directions (unpatched reader's test
correctly FAILS via `git stash`; patched reader's test passes). Full root
`ctest`: 135 of 135 (was 134). Fourth: **`AUD-067`** (commit `b2946f9`) —
`MeshBuilder.cpp`'s `frameAxes()` divided by its own binormal length with
no zero-guard (unlike `norm3()` a few lines above it in the same file,
which does guard) — a Polyline extrude path with two consecutive
identical `<point>` elements produces a `{0,0,0}` tangent (the neighboring
Bezier path already falls back to `{0,1,0}` for this exact case; Polyline
had no such fallback), dividing `0/0` into NaN that aborted the ENTIRE
export with a generic "non-finite vertex position" error. Fixed by
guarding the division (`b` stays `{0,0,0}` when degenerate — sufficient,
since the resulting normal `n=cross(b,t)` is always `{0,0,0}` when
`t=={0,0,0}` regardless of `b`, so the honest result is one pinched-but-
finite ring, not a NaN-corrupted mesh). New
`mc3togltf_degenerate_polyline_extrude` test parses the exported GLB's
POSITION/NORMAL accessors and confirms every float is finite; verified
both directions via `git stash` (unpatched: export fails with exit 2;
patched: succeeds, 312 vertices/164 triangles). Full root `ctest`: 136 of
136 (was 135). Fifth: **`AUD-068`** (commit `491f278`) —
`Mc3JsonParser::parseString()`'s `Mc3LoadPolicy` parameter was entirely
unused (`/*policy*/`) — `confineResourcePathsToRoot` was a silent no-op on
the `.mc3.json` load path (6 resource-path fields: mesh source, texture
uri, SVG src, embed src, sound src, music src), unlike the already-hardened
XML path (`AUD-006b`). No production call site currently passes
`untrusted()` to this parser, so latent, not yet exploited. Fixed by
mirroring `Mc3XmlParser.cpp`'s own `g_confineResourcePaths`/
`g_resourceRoot`/`includePathWithinRoot` exactly. Deliberately scoped to
`confineResourcePathsToRoot` only — `.mc3.json`'s `includes` field is an
inert passthrough list, never resolved/merged anywhere, so
`allowIncludes`/`confineIncludesToRoot`/`maxIncludeDepth` have nothing to
enforce on this path. New `mc3_json_load_policy` test (12 assertions);
verified both directions via `git stash` (unpatched: 7 confinement
assertions fail; patched: all 12 pass). Full root `ctest`: 137 of 137 (was
136); standalone CNA-free `mc3/build`: 21 of 21 (was 20). **New finding,
filed separately as `AUD-069` (P3/TODO, not fixed):** while writing this
fix's test, found a pre-existing latent bug SHARED with the XML parser's
`includePathWithinRoot` — a same-directory relative resource reference is
wrongly rejected as "escaping the root" when the target file doesn't
exist on disk AND the document was opened via a bare relative filename
(empty `sourceDir`); `weakly_canonical()` only resolves existing paths, so
a non-existent relative candidate stays unresolved and produces a
degenerate `relative()` result. Fails SAFE (a false rejection, not a
bypass) — low severity, not fixed here to keep `AUD-068` scoped to
porting the existing, verified XML behavior, not improving on it. Sixth:
**`AUD-070`** (commit `c3d5875`) — `Mc3XmlWriter.cpp` wrote SVG
inlineContent/script source/embed base64Content into a CDATA section
unconditionally, with no scan for an embedded `]]>` (a CDATA section
cannot contain its own closing delimiter). Content containing `]]>`
followed by attacker-chosen text closed the CDATA early and let the rest
be parsed as literal XML on the next load — empirically confirmed WORSE
than "garbled content": the unpatched writer's output for a crafted
payload is XML so broken it doesn't even parse back on reload
(`XML_ERROR_MISMATCHED_ELEMENT`), crashing the pre-fix test on an
uncaught exception (verified via `git stash`). Fixed with
`appendTextOrCData()`: CDATA when safe, entity-escaped plain text when
`]]>` is present. Deliberately NOT multi-CDATA-section splitting (the
other standard technique) — `Mc3XmlParser.cpp`'s `GetText()` only reads
the first child text node and would silently truncate a split payload.
New `mc3_cdata_injection` test (7 assertions) round-trips a payload
containing `]]>` + a fake injected `<object>` and confirms byte-identical
round-trip with no spurious object created. Full root `ctest`: 138 of 138
(was 137); standalone CNA-free `mc3/build`: 22 of 22 (was 21). Seventh:
**`AUD-071`** (commit `c70ba77`) — `McbReader.cpp`'s `readSceneState()`
(`overrides` key) and `readTrigger()` (`steps` key) were the only two
known keys left using `if (k == "X" && tag == TAG_ARR) {...} else {
skipValue(...) }` instead of `expectTag()` (`AUD-015`'s own established
policy) — a corrupted file with the right key but wrong tag byte silently
dropped the field instead of being rejected. Fixed by switching both to
the same `expectTag()`-then-read pattern every other known key uses. New
`mcb_tag_mismatch_rejection` test (12 assertions) patches the tag byte
right after each key's exact byte pattern from `TAG_ARR` to `TAG_STR` and
confirms loading now throws `"MCB: type mismatch for key '...'"` instead
of silently succeeding; verified both directions via `git stash`. Full
root `ctest`: 139 of 139 (was 138). Eighth: **`AUD-072`** (commit
`ea31df7`) — `SceneRenderer_Extrude.cpp`'s `drawExtrudeDynamic()`, the
neighboring function to `AUD-064`'s `drawGridDynamic`, had the same class
of bug: path segments × cross-section points combine multiplicatively and
used to be rebuilt (trig-computed + allocated) from scratch every frame
before the existing `numVerts>65535` bailout discarded the result —
`ex.segments="4096"` with a `4096`-segment circular cross-section built
the full ~16.8M-vertex buffer every frame. Empirically confirmed as a
real freeze (unpatched: did not finish within 25s; patched: ~2.3s, via
`git stash`). Fixed by computing the same vertex-count formula up front
and bailing before any allocation, mirroring `AUD-064`'s fix exactly; the
old end-of-function check is now provably unreachable but left in place
as a documented defensive backstop. New `test/extrude_stress.mc3.xml` +
`smoke_test_extrude_stress` ctest. Full root `ctest`: 140 of 140 (was
139). Ninth: **`AUD-073`** (commit `3af58fe`) — the same
`SceneRenderer_Extrude.cpp` file's hollow-cross-section `innerScale`
formula (`drawExtrudeDynamic()` and `drawObjectEdges()`, both identical)
divided by `cs.radius` with no zero-guard; `radius=0` is legal, and a
hollow cross-section (`innerRadius>0`) with `radius=0` made `innerScale`
evaluate to `+inf`, producing NaN vertices in both the solid viewport and
the edge-overlay wireframe. `mc3togltf`'s own export-side `buildExtrude()`
was already safe (`innerRadius < radius` can never hold when
`radius=0`) — editor-only gap. Fixed by requiring `radius > 1e-6f` before
treating a cross-section as hollow, matching `AUD-067`'s epsilon
convention. New `extrude_hollow_zero_radius_test` (12 assertions) mirrors
the exact formula (SceneRenderer is CNA-coupled, can't be unit-tested
directly) and concretely reproduces `+inf` from the pre-fix formula
before proving the fix stays finite. **Honestly documented, not silently
dropped:** a live-render screenshot comparison was tried first and found
to NOT discriminate this bug at all — a `radius=0` cross-section renders
byte-identical output either way, because the outer ring is already fully
degenerate/invisible when `radius=0` regardless of the inner-ring NaN, for
a reason unrelated to this fix. Full root `ctest`: 141 of 141 (was 140).
The remaining 3 findings from this same audit pass are not yet actioned —
see the user/session transcript for the full ranked list; re-derive from
a fresh audit if this note has gone stale rather than trusting it
indefinitely.

_Last updated: 2026-07-18 (later same day, third continuation this date).
Continues the two 2026-07-18 sessions recorded below (SYS-W14-10..17 +
SYS-W12-02 batch, then SYS-W14-16's undo/redo audit) with two further
autonomous stretches: (1) `SYS-W3-01` Phases 3-7 (`MacroRecorder`,
`UndoManager`, animation-override computation, `WalkController`,
`AudioPreview`), `SYS-W14-03` (real PNG screenshots), `SYS-W14-08` (AI
apply diff), and closing the stale `SYS-W9-01` row; (2) investigating and
declining the two remaining `SYS-W3-01` roadmap candidates (file dialogs,
post-processing) for different reasons — see §3's newest entries and §8.
This file's own commit is, as always, self-referential (it cannot cite its
own hash), see any `NEXT.md`-only commit for the same pattern. Working tree
clean except the same two untracked, unrelated scratch scene files noted
previously (`test/crownspire-citadel.mc3.xml`, `test/house3.glb` —
manually authored demo content, not part of any tracked task, left as-is).
Pushed to `origin/develop`; see `git log --oneline -20` for the exact
commit list newer than this._

**The §4/§5 "CNA build regression" described below is long RESOLVED** —
see the update at the top of §4 (a DIFFERENT, unrelated blocker affecting
`../easy-gl`/`../meta-gl` is open as of this same day's later top-of-file
note, read that before trusting any current full-suite count). At the
point this paragraph was written (right after `AUD-064`, before `AUD-065`
and before the new blocker), a fresh reconfigure + full rebuild + ctest had
133 of 133 tests passing, up from 126 of 126 at the start of the
referenced session (+1 from `AUD-064`'s `smoke_test_grid_stress` above).

**Today ran in three parts, all on `origin/develop`:** (1) the 9
`SYS-W14-10`..`17` + `SYS-W12-02` tasks, implemented one at a time per
`CLAUDE.md`'s ask-before-each workflow; (2) `SYS-W14-16`'s dedicated
undo/redo audit (two parallel research agents, 27 real gaps found+fixed);
(3) an explicitly user-authorized autonomous stretch across two
sub-sessions completing `SYS-W3-01` Phases 3-7, `SYS-W14-03`, `SYS-W14-08`,
closing stale `SYS-W9-01`, and investigating (then declining) file dialogs
and post-processing as further `SYS-W3-01` candidates. Build parallelism
was deliberately throttled to `-j4` for the back half of today (shared
machine running many concurrent sessions — see the RAM-caution note a
`free -h` check surfaced mid-session) rather than the earlier `-j$(nproc)`.
No external commits landed from the sibling `mesh-world` repo today
(checked at the end, not just the start).

**Human decisions obtained at the start of this session** (see `plan.md`'s
`SYS-W1-04`/`SYS-W3-01`/`AUD-036c` entries for the full rationale) — **all
4 are now fully implemented**, not just decided:
1. `EditorViewport` — **deleted** (not finished wiring in). Done.
2. `SYS-W3-01` Phase 2 Preferences — **narrow** scope (theme/panel-open
   only; autosave/snap/grid fields stay put). Extracted as
   `Editor::Preferences`. Done.
3. Duplicate object ids (`SYS-W1-04`) — **warning-level `Mc3Validation`
   diagnostic**, parsing stays permissive. Implemented in
   `Mc3XmlParser.cpp`. Done.
4. Undo/redo selection (`AUD-036c` open item) — **restores** the pre-
   mutation selection (`SYS-W9-03`), not cleared. Implemented in
   `MeshCraftApplication_Commands.cpp`. Done.

---

## 1. Project summary

**MeshCraft** is a desktop 3D scene editor (C++23, built on the CNA game
framework — an XNA/FNA-style API over SDL3 + OpenGL ES) for **MC3**, this
project's own scene/model format. A scene is a document (`Mc3Document`) of
primitives, CSG operations, materials, lights, cameras, animation, and
several extension namespaces (scripts, sounds/music, triggers, scene
states). The same in-memory AST has two serialization surfaces: `.mc3.xml`
(original) and `.mc3.json` (added recently, genuinely semantic JSON, not a
mechanical XML mirror). A separate binary format, `.mcb`, and a glTF/GLB
exporter (`mc3togltf`) round out the format family.

**Main goal (current phase):** this project has been in a **stabilization
phase** for several sessions — an evidence-based audit (`AUD-###` findings)
plus a systematic hardening workstream (`SYS-###`), not new user-facing
features. That backlog is now almost fully closed: every `AUD-###` finding
that isn't externally blocked is done, and of the systematic workstream only
one item remains, itself broken into phases (see §4).

**Important architectural decisions:**
- `Mc3Document` (in `mc3/`) is the single canonical in-memory AST. It has no
  CNA/GUI dependency and is shared by both parsers/writers, `mcb`,
  `mc3togltf`, `mc3tomcb`, and the editor.
- The editor (`MeshCraftApplication`) and the exporter (`mc3togltf`) are
  **two independent geometry generators** reading the same `Mc3Document` —
  there is no shared mesh-building code between "what you see in the editor"
  and "what gets exported." This is a deliberate but risky duplication (see
  §6).
- `../cna` and `../sharp-runtime` are **sibling repositories, not part of
  this repo, and not to be modified from here** — a separate process/owner
  handles them. This repo only consumes them via `add_subdirectory`.
- A second, unrelated sibling repository, **`../mesh-world`** (a
  procedurally-generated 3D world explorer built on top of MC3), drives its
  own feature work directly into this repo's `mc3/` library from time to
  time (see §3 and §5) — commits can land here that this repo's own
  `plan.md` never asked for.

## 2. Current status

- **Build: clean, RE-VERIFIED against a genuine fresh reconfigure.** A
  `../cna`-side regression briefly broke every fresh build of this repo
  mid-session (see §4 for the full history) — **fixed in `../cna` commit
  `730ebbe9`** (with this session's own user's explicit authorization,
  since it required touching the normally-out-of-bounds sibling repo).
  Re-verified after the fix: fresh `cmake -S . -B b-release` + full
  `cmake --build b-release -j"$(nproc)"` from a clean reconfigure, zero
  errors/warnings.
  ```bash
  cmake -S . -B b-release && cmake --build b-release -j"$(nproc)"
  ```
- **Tests: 132 / 132 `ctest` passing**, re-run against a fresh build as of
  today's last commit. Net +6 tests since this morning's 126/126
  (`macro_recorder`, `undo_manager`, `walk_controller`, `audio_preview`,
  `png_screenshot_test`, plus assertions added to existing binaries
  `ai_test`/`mc3_commands_test`/`undo_gesture_frame_test` — see §3).
- **CLI/tools/apps/libraries currently available:**
  - `MeshCraft` — the interactive editor (`./b-release/MeshCraft
    scene.mc3.xml`, or `--screenshot out.png` / `--export out.glb` for
    headless one-shot runs).
  - `mc3togltf` — MC3 → glTF/GLB exporter (`--stats` prints export +
    pre-export-validation diagnostics).
  - `mc3tomcb` — MC3 XML → MCB binary converter.
  - Standalone libraries `mc3` (format/AST + XML/JSON parse-writer),
    `mcb` (binary format) — both buildable and testable without CNA.
- **Recently implemented (today, 2026-07-18; see §3 for the exact commit
  list):** the 8 `SYS-W14-10..17` editor-UI-coverage tasks + `SYS-W12-02`
  (`--benchmark` mode); `SYS-W14-16`'s undo/redo audit (27 real gaps
  found+fixed: 6 missing `pushUndo()` calls, 21 `AUD-036`-style dead
  patterns); `SYS-W3-01` Phases 3-7 — extracted `Editor::MacroRecorder`,
  `Editor::UndoManager`, the animation-override computation
  (`computeAnimOverridesAlg`), `Editor::WalkController`, and
  `Editor::AudioPreview` out of `MeshCraftApplication`; `SYS-W14-03` (real
  PNG encoding for `--screenshot` — it used to silently write raw PPM
  under a `.png` name, despite `--help` promising PNG); `SYS-W14-08` (AI
  apply preview/diff before "Apply to Scene"); closed stale `SYS-W9-01`
  (already resolved via `AUD-036b`+`SYS-W9-03`+`SYS-W14-16`, left
  incorrectly marked `TODO`/`P0`). File dialogs and post-processing
  (bloom/skybox/SSAO) were investigated as further `SYS-W3-01` candidates
  and explicitly declined — see §4/§8.
- **Known working examples:** `./b-release/MeshCraft test/house.mc3.xml`;
  `./b-release/MeshCraft <scene> --screenshot out.png` (now writes a real,
  decodable PNG — `SYS-W14-03` fixed the long-standing "always raw PPM
  regardless of extension" gap; genuinely captures the composited
  ImGui+3D framebuffer, not just the viewport); `./b-release/MeshCraft
  <scene> --benchmark` (`SYS-W12-02`, in-process timing for
  startup/frame/traversal/picking/undo/anim/registry);
  `./b-release/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb`.
- **What does not work yet / is not verified:**
  - Web (Emscripten) build: blocked by a crash inside `../cna`, not this
    repo (see §4/§5).
  - Windows (MinGW): does not compile — 1 remaining failure, in `../cna`.
  - CI: present and believed correct (`.github_/workflows/ci.yml`) but
    parked under a non-standard directory name and never actually runs
    (owner-gated — needs a workflow-scoped push token).

## 3. Recent changes

**2026-07-18 (today, three parts — see the top-of-file summary):**

- **Part 1 — the 8 `SYS-W14-10..17` rows + `SYS-W12-02`, all DONE:**
  `scriptId` object-attachment combo (`-10`); native file-browse dialog
  via `CNA::Devices::FileDialog`, new `CNA_DEVICES` CMake option (`-15`);
  wired up write-only `coordinate_system` into the rotation-convention
  status-bar notice (`-14`); `.mc3.json` editor open/save, plus a real
  pre-existing bug fix (`saveFile()` always wrote XML regardless of the
  current file's actual extension) (`-11`); Area properties panel
  investigation, closed with no further UI needed (`-17`); Library/Imports
  UI — new Scene Properties section + "Imports" tab (`-13`); `assetMetadata`
  editor, all 23 fields, new "Defs" tab tree node (`-12`); `--benchmark`
  headless CLI mode timing 7 of 10 categories directly (`SYS-W12-02`).
- **Part 2 — `SYS-W14-16`, undo/redo structural-guarantee audit, DONE:**
  two parallel research agents (command/input/macro files; panel/UI files)
  found 27 real gaps: 6 missing `pushUndo()` calls (`toggleIsolate()`,
  Anim Loop/Autoplay checkboxes, macro `show_all`, Default Camera combo,
  States/UV rotation fields) and 21 `AUD-036`-style dead patterns
  (`IsItemActivated()` nested inside a Slider/Drag changed-check across
  every primitive segment slider + both Material tabs' Roughness/Metallic/
  Alpha-Cutoff sliders). All fixed; a mechanical grep confirmed zero
  remaining instances. 127/127 `ctest`.
- **Part 3 — `SYS-W3-01` Phases 3-7 + two standalone features + one stale
  closure, all DONE (user explicitly authorized an autonomous run, asked
  "co potrebujes"/checked in twice for genuinely riskier decisions):**
  - **Phase 3:** `Editor::MacroRecorder` — the harder of the two Phase-1-
    deferred candidates; resolved with a `PropertiesContext`-style
    callback `Context` built per call site (`macroContext()`), not stored.
  - **Phase 4:** `Editor::UndoManager` — found a 4th real consumer beyond
    push/undo/redo by grepping first (the Undo History dialog's
    jump-to-step logic). Needed NO callback Context at all, unlike Macro.
  - **`SYS-W14-03` DONE:** real PNG encoding for `--screenshot` (reused
    `mc3togltf_lib`'s already-compiled `stb_image_write.h`, zero new
    dependency) — it had always silently written raw PPM under a `.png`
    name despite `--help` promising PNG.
  - **`SYS-W14-08` DONE:** AI apply preview/diff
    (`computeAiChangeSummaryAlg`) — added/removed/modified object summary
    shown before "Apply to Scene", explicitly scoped to
    name/type/visible/material/transform, not exhaustive.
  - **`SYS-W9-01` closed as DONE** (docs-only): was a stale pointer-only
    row to `AUD-036b`, already resolved via `AUD-036b`+`SYS-W9-03`+this
    same day's `SYS-W14-16`.
  - **Phase 5 (narrowed):** only `evaluateAndPushAnimOverrides()`'s pure
    computation extracted (`computeAnimOverridesAlg`), not the ~800-line
    timeline UI — same narrowing judgment as Phase 2's Preferences.
  - **File dialogs investigated, declined:** already well-factored thin UI
    glue over existing tested `Alg` functions, zero testability payoff.
  - **Phase 6:** `Editor::WalkController` — the cleanest extraction of the
    series (zero `document_`/`selection_` coupling). Found and removed
    genuinely dead code (`walkSettingsOpen_`) along the way.
  - **Phase 7:** `Editor::AudioPreview` — small, self-contained, a real
    correctness constraint worth encapsulating (loop must be set before
    `Play()`) plus a real duplicated `isPlaying()` check unified.
  - **Post-processing (bloom/skybox/SSAO) investigated, declined:** 100%
    side-effecting raw GL calls sharing one fragile shared resource
    bundle, with a documented prior bug (see `project_bloom_bug` memory).
    No pure logic to extract; correctness only verifiable by real pixel
    comparison, not unit tests — declined rather than force the risk.
  - 132/132 `ctest` at the end of today. Build parallelism throttled to
    `-j4` partway through (shared machine, many concurrent sessions).

**New autonomous session, 2026-07-17** (continues from the session below;
see the top-of-file decisions block for the 4 human-authorized decisions
this session started with):

- Recorded the 4 decisions in `plan.md` (`SYS-W1-04`, `SYS-W3-01`,
  `AUD-036c`, new `SYS-W9-03`) and here.
- **`SYS-W6-04` (new, DONE):** closed `field_matrix`'s 18-field gap for
  real — `Mc3Object::assetMetadata` (R111), `Mc3Object::scriptId` (R103),
  and `Mc3Document::library`/`imports` (R110/R101) were completely absent
  from both `mc3.xsd` (not a lint nitpick — any document actually using
  `<assetMetadata>`/`<library>`/`<imports>` failed XSD validation outright)
  and MCB (silent XML/JSON→MCB→XML data loss for the same features). Added
  the missing XSD types/elements and `McbReader.cpp`/`McbWriter.cpp`
  read/write support; the 4 residual field_matrix rows
  (`max_visibility_distance`/`namespace`/`script`/`tier`) are genuine
  naming-convention asymmetries, now allowlisted with reasons. New fixture
  `test/asset_metadata_library_import.mc3.xml` + 3 new
  `mcb_roundtrip_test` functions (~40 assertions). Full details in
  `plan.md`'s `SYS-W6-04` entry. **123/123 ctest** (was 122/123).
- **Documentation fix:** `AUD-015`'s header line still said "PARTIAL: ...
  27 sibling read* functions remain" despite the entry's own later
  "Resolved"/completion notes showing it was fully finished in a prior
  session (commit `00aee08`, 189 `expectTag` call sites). Corrected the
  header; also flipped `SYS-W6-01` from stale `[TODO]` to `[DONE]` since
  all 3 of its constituent `AUD-###` findings were already independently
  `[DONE]`. No code change from this fix, `readSceneState`'s deliberate
  skip-not-throw exception (see its own plan.md note) was left as-is after
  a brief revert (see git history on this file if curious — not worth its
  own bullet).
- **`SYS-W1-04` (now DONE):** implemented the decided duplicate-object-id
  handling — `checkDuplicateObjectIds()` in `Mc3XmlParser.cpp` (called at
  the end of `buildDocumentFromRoot()`) walks `doc.objects` + recursive
  `.children` (the same scope `flatFindById` searches) and emits one
  warning-level `Mc3Validation` diagnostic per duplicated id. Parsing stays
  fully permissive. 4 new assertions in `mc3_duplicate_ids_test.cpp`.
- **`EditorViewport` deleted:** removed the abandoned-scaffolding stub
  (`include/MeshCraft/Editor/EditorViewport.hpp` +
  `src/MeshCraft/Editor/EditorViewport.cpp`) outright — zero references
  anywhere else, confirmed before deleting. Sources are globbed, so a
  `cmake .` reconfigure (not a `CMakeLists.txt` edit) was needed to pick up
  the removal. `camera_`/`gizmo_` remain separate `MeshCraftApplication`
  members, unchanged.
- **`SYS-W3-01` Phase 2 (narrow) DONE:** extracted `Editor::Preferences`
  (theme + Preferences-dialog-open state + `applyTheme()` only —
  `autoSaveInterval_`/`snap*`/`gridSpacing_` deliberately stay on
  `MeshCraftApplication`, per the narrow-scope decision).
  `loadPrefs()`/`savePrefs()` stay on `MeshCraftApplication` too, since
  they persist the theme together with those cross-domain fields in one
  prefs file. New `preferences_test` (no coverage existed before). Full
  rebuild + **124/124 ctest** (new `preferences` test); manual
  `--screenshot` smoke test confirms the app still boots/renders.
- **`SYS-W9-03` DONE:** `performUndo()`/`performRedo()` now restore the
  pre-mutation selection instead of clearing it. New
  `undoSelectionStack_`/`redoSelectionStack_` kept in lockstep with
  `undoStack_`/`redoStack_` everywhere those are pushed/popped/cleared
  (including the "Undo History" jump-to-step dialog and the 3 file-load
  `.clear()` sites). Restore is by id (`flatFindSharedById()`, a new
  shared-ptr counterpart of `flatFindById()`), silently skipping an id no
  longer present post-swap rather than dangling/crashing. 5 new mirror-
  model assertions in `test/undo_gesture_frame_test.cpp` (the real
  functions are CNA-coupled and not headlessly callable, matching that
  file's existing convention for this class of test). Full rebuild +
  **124/124 ctest**; manual `--screenshot` smoke test confirms the app
  still boots/renders.
- **Doc fix:** `AUD-014` ("join the AiAssistant background thread at
  shutdown") was already fully fixed in an earlier session (commit
  `3cd27d7`, `plan.md` already read `[DONE]`) — only this file's §5/§8
  hadn't caught up. Corrected, no code change. Same class of staleness as
  the `AUD-015`/`SYS-W6-01` fix above.
- **Stale-`TODO` sweep:** after finding `AUD-014`/`AUD-015`/`SYS-W6-01`
  stale earlier this session, did a quick pass over the remaining
  `SYS-###` `TODO` rows and found 3 more: `SYS-W2-03` (its `AUD-009` has
  read `[DONE]` since commit `ca82b0b`), `SYS-W6-03` (its `AUD-018` has
  read `[DONE]` since commit `ba3e73c`), `SYS-W14-01` (duplicate of the
  already-`[DONE]` `SYS-W9-02`, autosave+crash-recovery). All 3 flipped to
  `[DONE]` with a status note, no code change. While re-verifying
  `SYS-W6-03`, found this session's own earlier `SYS-W6-04` work had
  freshly re-broken `MCB_FORMAT.md`'s field-order list (missing the new
  `library`/`imports` fields) — fixed that too.
- **`SYS-W5-02` DONE:** documented every `MC3_FORMAT.md` gap
  `xsd_docs_diff.py` reports (grew from the original 5+12 to 14+25 by the
  time this was picked up, partly from this session's own `SYS-W6-04`
  additions) — new "Library and Imports"/"UV Mapping"/"Asset Metadata"
  sections plus several attribute-table additions. `xsd_docs_diff.py` now
  reports 0 gaps (was 14 elements + 25 attributes).
- **`SYS-W2-04` (now DONE, one sub-point deliberately left open):**
  "extracted-XML size" turned out to already be bounded (`SYS-W1-03`'s
  512MB `checkDocumentByteBudget()` already runs inside the AI-apply
  path's `loadFromString()` call — verified, not re-implemented). Added
  `AiAssistant::redactSecret()`/`boundedForDisplay()` (both exposed as
  public static methods, matching the existing `jsonEscape` convention):
  every caught-exception error message in `sendAsync()` now has the API
  key redacted (defense-in-depth — no current path actually leaked it,
  but nothing structurally prevented a future one from doing so) and any
  embedded HTTP response body capped at 4KB with a truncation note. New
  pure-function tests plus a real mock-`httplib::Server` integration test
  proving a 20000-byte error body produces a ~4KB `errorMsg()`.
  At the time, capping the response size during the actual network *read*
  was deliberately left open as new `SYS-W2-05` — **closed later this same
  session, see below.**
- **`SYS-W2-05` DONE (closes the gap `SYS-W2-04` left open):** replaced
  `sendAsync()`'s `cli.Post(...)` convenience call with a raw
  `httplib::Request` sent via `cli.send(req)`, with a `content_receiver`
  that aborts the read once a response exceeds the new public
  `AiAssistant::maxResponseBytes` field (default 8MB, overridable like the
  existing timeout fields). Response body genuinely never grows past the
  cap in memory now, not just when later displayed. 2 new mock-server
  tests (cap-exceeded aborts cleanly; comfortably-under-cap still
  succeeds); every pre-existing `ai_test.cpp` mock-server test still
  passes unmodified, confirming the rewrite is a faithful `Post()`
  replacement. Full rebuild + 124/124 `ctest`; manual `--screenshot`
  smoke test.

- **`SYS-W1-05` DONE (partial, honestly scoped):** added a 256-deep
  recursion guard (matching `GltfExporter.cpp`'s existing `kMaxNodeDepth`
  precedent) to `deepCopyObjectAlg` and `deepCopyObj`/`deepCopyDoc` — the
  two most frequently-invoked recursive walks over `Mc3Object::children`
  (called directly by Duplicate/Group/undo on every action) — so a cyclic
  children graph (only possible via C++-API misuse; XML parsing can't
  produce one) now throws a clean error instead of stack-overflow-
  crashing. 2 new tests (2-cycle, self-cycle). Several other recursive
  walkers remain unguarded (`Mc3XmlWriter`, `SceneRenderer`, `mc3togltf`'s
  `MeshBuilder`) — tracked as new `SYS-W1-06`, lower priority since a
  cycle there would surface via a less-immediate operation. Full rebuild
  + 124/124 `ctest`; manual `--screenshot`/`--export` smoke tests.

- **`SYS-W12-01` DONE (Phase 1 of 2):** new `test/benchmark.py` times
  `mc3tomcb`/`mc3togltf` CLI tools (median of N runs) against small/
  medium/large real fixtures — XML↔MCB convert + glTF export, 3 of the
  13 originally-named categories. New `benchmark` ctest, informational
  only (never fails on timing, only on a crash — wall-clock timing on a
  shared machine is too noisy for a hard threshold to be a real signal).
  Baseline numbers recorded in new `test/BENCHMARK_BASELINE.md`. The
  remaining 10 categories need in-process instrumentation, not just
  CLI timing — tracked as new `SYS-W12-02`. Full rebuild + **125/125
  ctest** (new `benchmark` test).

- **`SYS-W5-05` DONE (7th stale finding this session):** duplicate of the
  already-`[DONE]` `SYS-W11-05` — property-based round-trip
  (`mc3_random_roundtrip`) and fuzz (`mc3_xml_mutation_fuzz`,
  `mcb_random_roundtrip`, plus standalone libFuzzer harnesses) coverage
  already exists and is ctest-registered. No new work.
- **`SYS-W5-03` investigated + resolved (human-authorized decision,
  2026-07-17):** confirmed with a live test that MC3 round-tripping
  silently drops any unrecognized XML attribute/element (root, object, or
  a whole unknown element) — by design, not a bug: the parser reads only
  named known fields, the writer rebuilds the element from scratch
  emitting only known fields, there's no "preserve what I don't
  recognize" codepath anywhere. **Decision: leave as-is, document as an
  accepted limitation.** No implementation. Documented in `MC3_FORMAT.md`
  (new "Forward/backward compatibility" section) and `plan.md` (now
  `DEFERRED`).
- **`SYS-W5-04` investigated, left `TODO` (correctly, not stale):**
  confirmed 13 real call sites doing independent O(n) tree-walk id/name
  lookups with zero caching/indexing anywhere. Deliberately not attempted
  in this pass — a correct cached index needs careful invalidation
  analysis across every `document_`-mutating call site first (the same
  research-before-implementing discipline `SYS-W3-01` used), a bigger,
  riskier task than fits one "continue working" iteration. **Done later
  the same day, third pickup this session — see its own `DONE` entry
  further down.**

- **`SYS-W1-06` DONE (3 walkers guarded):** extended `SYS-W1-05`'s
  256-deep cycle guard to `findParentListAlg`/`removeFromListAlg`
  (Delete/reparenting) and — most importantly — `Mc3XmlWriter::writeObject`,
  the actual Save/Export path. New test confirms `saveToFile()` throws a
  clean error instead of crashing on a cyclic document. `SceneRenderer`
  remains unguarded (needs a live GL context to test properly, lower
  marginal value since a cycle would already be caught earlier by
  `deepCopyObjectAlg` in virtually every real workflow) — tracked as new
  `SYS-W1-07`. (The export path's own recursion, `GltfExporter.cpp`'s
  `buildNode()`, turned out to already be guarded by a pre-existing,
  pre-this-session `AUD-007` fix — corrected a same-session mistake in
  `SYS-W1-07`'s own scope, see its `plan.md` entry.) Full rebuild +
  125/125 `ctest` (stable across repeats);
  manual `--screenshot` smoke test.

- **`SYS-W7-01` DONE (8th stale finding this session):** all 3 cited
  `AUD-###` findings (`024`/`026`/`029`) were already `[DONE]` — only
  `MC3_FORMAT.md`'s pre-existing export support matrix table had never
  been updated to reflect them. Added 3 rows (UV mapping, per-object
  `metadata`, `--stats` warning-count truthfulness), each verified
  against current `GltfExporter.cpp` source directly. "Machine-checked"
  wasn't newly built as dedicated tooling (documented as a real, separate,
  larger undertaking if wanted later — `field_matrix.py` already has a
  partial informational heuristic covering similar ground). Doc-only;
  125/125 `ctest`.

- **`SYS-W11-07` DONE (offline half) / genuinely blocked (package-first
  half):** every fetched dependency was already pinned to an exact
  `GIT_TAG`, so a fully offline build turned out to already work via
  CMake's built-in `FETCHCONTENT_SOURCE_DIR_<NAME>` override — zero code
  change, just documentation (verified empirically first: a real
  `-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=<path>` configure reused the
  local dir with no clone attempted). New `README.md` "Offline / vendored
  build" section with the full dependency table + a ready configure
  command. Package-first discovery (`find_package(CNA)`) is genuinely
  blocked, not deferred: `cna` has no `install()`/package-config export to
  find, and adding one is a `cna`-side change out of bounds per
  `CLAUDE.md`. Doc-only; 125/125 `ctest`.

- **`SYS-W1-07` DONE (continuation, second half of this same session):**
  actually read `SceneRenderer.cpp`/`SceneRenderer_Extrude.cpp`/
  `CsgCacheAlg.hpp` in full instead of estimating from the row's one-line
  description — of the ~11 recursive `Mc3Object::children` traversal call
  sites, 7 were already guarded (`buildManifoldTree`/`csgSubtreeWarning`/
  `csgSubtreeHashAlg` at `depth > 12`; `drawObject`/`drawEmissiveObject`/
  `drawObjectEdges` at `depth > 16`) and only 4 local lambda helpers had no
  guard at all: `computeObjectWorldMatrix`'s `find`, `drawCsgGizmos`'s
  `visit`, and `objectPolyStats`/`scenePolyStats`'s `walk`. Added a
  `depth` parameter to each, capped at the same `depth > 16` this file
  already uses for equivalent non-CSG object-tree recursion. No public
  API change. Verify: full rebuild + 125/125 `ctest`, plus a real live-GL
  `--screenshot` before/after byte-diff (identical) on `test/house.mc3.xml`,
  `test/csg_cache.mc3.xml`, `test/features.mc3.xml` — see `plan.md`'s
  `SYS-W1-07` entry for the full per-function breakdown. **While verifying
  this, discovered the CNA build regression described in §4 — unrelated to
  this fix (reproduces identically with it reverted), not fixed here.**

- **`SYS-W5-04` DONE (third pickup this session):** ran the "scoped
  invalidation-analysis pass" this row itself said it needed — two parallel
  research agents independently enumerated (1) every id/name lookup call
  site with a per-frame-vs-one-shot classification and (2) every
  `document_`-mutating call site (add/remove/rename/reparent/wholesale-
  replace), each cross-checked against the real source before any code was
  written. **Key finding that reshaped the fix:** of the 13+ lookup call
  sites the row originally cited, only ONE is a genuine per-frame hot path
  — `evaluateAndPushAnimOverrides()`'s `flatFindByName` calls, re-resolving
  the same animation-channel target names every frame during playback.
  Every other call site is one-shot per user action (Delete, Break
  Instance, Registry Insert, drag-and-drop, batch rename, Find & Replace,
  ...), where an O(n) walk is negligible even on a large scene — indexing
  those too would add real invalidation-correctness risk for zero
  measurable benefit, so this session deliberately scoped the fix down to
  just the 3 already-existing accessor methods that already serve the one
  real hot path. New `Editor::ObjectIndex` (CNA-free,
  `include/MeshCraft/Editor/ObjectIndex.hpp` +
  `src/MeshCraft/Editor/ObjectIndex.cpp`, matching the `SelectionManager`
  narrow-owned-helper idiom): an `unordered_map`-backed id/name→object
  cache over `doc.objects` (not `doc.definitions`, matching
  `flatFindById`'s own existing scope), rebuilt lazily on the next lookup
  after `invalidate()`. Pre-order build reproduces
  `flatFindById`/`flatFindByName`'s existing "first match in document
  order" semantics exactly, including for documents with duplicate ids
  (`SYS-W1-04`) — verified by a dedicated test, not just assumed. New
  256-depth cyclic-children guard, matching `SYS-W1-05`/`06`/`07`'s
  convention for any new recursive `Mc3Object::children` walk.
  `flatFindById`/`flatFindSharedById`/`flatFindByName` now delegate to a
  new `objectIndex_` member — same signatures, zero call sites changed.
  Invalidation: one line in `pushUndo()` (confirmed via grep to run before
  virtually every mutating command across the whole `src/MeshCraft/` tree)
  plus one line at each of the 10 wholesale `document_ = ...` replacement
  sites (`performUndo`/`performRedo`, Undo History jump-to-step, both Open
  File branches, New Scene, autosave recovery, Open Recent File, startup
  load, AI-apply) — all 10 individually cross-checked file:line against
  the research pass's own independent enumeration. New
  `test/object_index_test.cpp` (`object_index` ctest, CNA-free, no `Mc3`
  library link needed) — 17 assertions covering nested-tree lookups,
  not-found cases, duplicate-id/name first-match ordering,
  invalidate-then-rebuild against a real mutation, and the cyclic-children
  guard. Full rebuild + **126/126 `ctest`** (was 125, +1 new test); manual
  `--screenshot` smoke tests on `test/house.mc3.xml` and
  `test/animation_demo.mc3.xml` (clean GL state). See `plan.md`'s
  `SYS-W5-04` entry for the full write-up.

**New day, 2026-07-18 (3 commits, continuing the same overall thread):**

- **Archived `plan.md`'s completed rows:** the file had grown to 1778
  lines, almost entirely `DONE` rows kept only for provenance. Moved every
  `DONE`-status `AUD-###` (61) and `SYS-###` (33) row out to
  `docs/history/plan_20260718.md` verbatim (full evidence/resolution text
  preserved, nothing summarized away) — plan.md is now 370-ish lines,
  tracking only its 6 still-open `AUD-###` rows (4 TODO, 2 DEFERRED) and
  (before this session's own additions) 13 still-open `SYS-###` rows.
  Fully-archived workstreams (W1, W2, W6, W7, W8) keep their heading with a
  pointer to the history file rather than silently disappearing. Verified
  every id lands in exactly one file (no losses/duplicates) before
  committing; `test/validate_plan_consistency.py` re-run clean against the
  much smaller file.
- **Re-verified `missing.md`** (an editor-UI-coverage-gaps audit dated
  2026-07-10) against current source via two parallel research passes.
  Finding: almost the whole file was stale — all 7 "zero UI" extension
  findings (N1-N7: SVG textures/embeds/scripts/audio/triggers/scene
  states/meta) and 6 of 12 "partial gap" findings had already been fixed in
  an earlier session this file's date predates (`STAB-0703`..`STAB-0721`).
  Updated the file in place: added a "Resolved since 2026-07-10" section
  for credit/history, refined the still-genuinely-open findings
  (`coordinate_system` is UI-valid now but still never read anywhere by
  rendering/export; `rotation_units`/`euler_order` gained a read-only
  load-time notice but still no editing UI, confirmed still won't-fix by
  design), and added 4 new findings for mc3 fields added since 2026-07-10
  that still have zero editor UI (`scriptId`, `assetMetadata`,
  `library`/`imports`, `.mc3.json` file I/O).
- **Added 8 new `plan.md` tasks** (`SYS-W14-10` through `SYS-W14-17`) from
  `missing.md`'s still-open findings, per the user's explicit selection —
  see §8 for the list and `plan.md`'s W14 section for full evidence per
  row.

**Prior session (11 commits, oldest first, all on `develop`, all pushed):**

- `d9e98d5` — fixed 2 pre-existing XSD-invalid test fixtures (unrelated
  double-hyphen-in-XML-comment and duplicate-`id` bugs found while
  re-verifying the baseline).
- `a119415` — **added** `Mc3Document::validate(Mc3Validation&)`: re-validates
  a document's current in-memory state by round-tripping it through the
  XML writer/parser, for documents that never went through a parsing load
  at all (built programmatically or mutated after loading). New
  `mc3_document_validate_test`.
- `92c6246` — wired the above into the AI-response path
  (`AiResponseAlgorithms.hpp`); extended `ai_test.cpp`.
- `297af3e` — wired it into `mc3togltf`'s `GltfExporter` (new `validation`
  member, printed under `--stats`); new
  `mc3togltf_pre_export_validation_test`.
- `899b486` — wired it into 4 real load call sites (startup, Open File,
  Open Recent File, autosave recovery) and into `saveFile()`.
- `f0b33b1` — docs: closed out `SYS-W1-01`.
- `9c7bfa5` — **added** a "Validation" ImGui panel + status-bar indicator
  (`MeshCraftApplication_UiValidation.cpp`) surfacing the diagnostics above
  in the UI, not just the console.
- `d388529` — docs: closed out `SYS-W14-02`.
- `c9b48a6` — **added** root `.clang-format` / `.clang-tidy` (config only —
  no file in the tree was reformatted; `clang-tidy` actually run and found
  only pre-existing, already-triaged issues).
- `d364712` — docs: closed out `SYS-W11-06`.
- `95aaa90` — **refactor:** extracted `Editor::KeybindingManager`
  (`include/MeshCraft/Editor/KeybindingManager.hpp` +
  `src/MeshCraft/Editor/KeybindingManager.cpp`) out of
  `MeshCraftApplication`; deleted `MeshCraftApplication_Keybindings.cpp`;
  updated ~90 call sites; new `keybinding_manager_test` (no coverage existed
  for this subsystem before).
- `d2264ad` — docs: recorded `SYS-W3-01`'s full scope/roadmap and marked
  Phase 1 done.

**Also present on `develop` but NOT done by this session** — landed from
the sibling `mesh-world` repo's own, separate backlog while this session
was in progress: `e7bed06` (R109, semantic `mc3.json`), `ff63ef5` (R110,
`.mc3lib` library format), `7110ebd` (R111, `Mc3Object::assetMetadata`),
`83819f8` (R101, `<imports>`), `df9d5ea` (R102, composite-object split),
`f392d41` (R103, script IDs). These are real and tested, but R111 (plus
R110/R101) left `mc3.xsd`/MCB gaps — closed this session, see §3's
`SYS-W6-04` entry above.

## 4. Current blocker / main problem

**RESOLVED, 2026-07-18 (same session, ~1 hour later): `../easy-gl`/`../meta-gl`
(two-level-deep siblings of `../cna`) were mid-edit and briefly broke the
full root build.** Discovered incidentally while verifying `AUD-065` (a
mc3-only, CNA-free fix) — NOT caused by this session's own work; confirmed
at the time by building the unrelated `Mc3` library and its tests in
isolation (both the standalone `mc3/build` tree and the root `b-release`
tree), which compiled and passed cleanly throughout. The failure was
entirely inside `../easy-gl`:

```
easy-gl/include/easygl/Device.hpp:50: error: 'ClearBuffer' has not been declared
easy-gl/src/Device.cpp:141: error: 'ClearBuffer' was not declared in this scope
```

At the time, `git status` in `../meta-gl` showed real uncommitted local
modifications to `include/metagl/Enums.hpp`/`EnumNames.hpp`/`Functions.hpp`
(adding/renaming `ClearBuffer`-related entries) with a very recent mtime —
another process's in-progress edit (per `CLAUDE.md`, a separate instance
handles CNA and its own dependency chain), not a committed regression.
`cmake --build b-release -j4 -- -k0` confirmed all 16 failing `.o` files
were inside `easy-gl` (`Buffer.cpp`, `Device.cpp`, `Program.cpp`,
`Texture.cpp`, ...) — nothing in `mesh-craft`'s own tree. Per `CLAUDE.md`
this stayed untouched from here (out of bounds, same as
`../cna`/`../sharp-runtime`) and, as predicted, resolved itself: re-checked
on the user's prompt, `../meta-gl`'s working tree is now clean (the
`ClearBuffer` enum landed for real, `include/metagl/Enums.hpp:1768`) and a
completely fresh `cmake -S . -B b-release` + `cmake --build b-release -j4`
compiles/links every target with zero errors, including `MeshCraft` itself.
**Full root `ctest -j4`: 134 of 134 tests passing** (`plan_consistency`
included), confirming both `AUD-064` and `AUD-065` are good end-to-end in
the real CNA-linked build, not just their CNA-free subsets.

**RESOLVED, 2026-07-17 (same session): a `../cna`-side regression briefly
broke every fresh build of this repo; fixed in `../cna` with the user's
explicit authorization.** Discovered while verifying `SYS-W1-07`. A plain

```bash
cmake -S . -B <any-dir>    # or anything that forces build.ninja to regenerate
cmake --build <any-dir> -j"$(nproc)"
```

failed compiling almost every `MeshCraftApplication*.cpp`/
`SceneRenderer*.cpp` file with:

```
CNA/GraphicsBackendType.hpp:93:2: error: #error "CNA: no CNA_BACKEND_*
compile definition set -- graphics backend selection
(cmake/BackendSelection.cmake) is broken"
```

**Confirmed not caused by this session's own work:** reproduced identically
against a clean checkout of this repo (SceneRenderer.cpp's changes fully
`git stash`ed out) — the only thing that changed is `../cna`'s `develop`
HEAD, which moved to `58e82fd3` ("Merge branch 'feature/graphics'",
D3D11/D3D12 PBR + skinned-vertex-color work) partway through this session.

**Root cause (traced and fixed):** `cna/cmake/BackendSelection.cmake` sets
the `CNA_BACKEND_EASYGL` macro via a directory-scoped `add_compile_definitions()`
call inside `../cna`'s own `CMakeLists.txt`. Directory-scoped compile
definitions propagate *down* into subdirectories `../cna` itself adds (like
`../easy-gl`), but never *up* to this repo's own targets — this repo adds
`../cna` via `add_subdirectory(../cna CNA_dep)`, so `../cna` is the child
here. `GraphicsBackendType.hpp` is a header included by both `../cna`'s
own `.cpp` files and this repo's — every translation unit needs the macro
at its *own* compile time. This gap was invisible until `../cna` commit
`2bd79fe9` (2026-07-17 06:45, landed on `develop` via the `feature/graphics`
merge at 20:46 the same day) added `GraphicsBackendType.hpp` itself — a
plain (non-template) header-defined function whose `#error` fires in
*every* including translation unit, immediately exposing the pre-existing
propagation gap as a hard build break for any consumer, this repo
included.

**Fix landed, 2026-07-17, in `../cna` commit `730ebbe9`** (pushed to
`origin/develop`, user explicitly authorized touching `../cna` for this
one change after asking exactly where the break was): each of
`BackendSelection.cmake`'s 14 backend branches now also does
`set(CNA_BACKEND_DEFINE "CNA_BACKEND_XXX")` next to its existing
`add_compile_definitions(...)` call, and `CnaLibrary.cmake`'s
`target_compile_definitions(CNA PUBLIC ...)` now includes
`${CNA_BACKEND_DEFINE}` — making the backend macro a real PUBLIC usage
requirement of the `CNA` target, so it now propagates through
`target_link_libraries` to any consumer regardless of directory scope.
`add_compile_definitions()` itself was left in place (still needed for
`../cna`'s own in-tree targets created before the `CNA` library target).
**Verified:** fresh `cmake -S . -B b-release` + full rebuild + 125/125
`ctest`, all green, on the EASYGL backend (the only one buildable in this
environment). All 14 backend branches checked for exact name-consistency
between `add_compile_definitions()` and `CNA_BACKEND_DEFINE` (all match);
the other 13 backends could not be built/tested here (no Vulkan SDK,
no Windows toolchain, etc.) but the change is structurally identical
across all of them. Pushing this fix also pushed **52 previously
local-only commits** already sitting in the `../cna` checkout (the full
`feature/graphics` PBR/skinned-vertex-color merge chain) to
`origin/develop` — worth knowing since it moved that remote branch much
further than this one fix commit alone.

`SYS-W3-01` (decompose the `MeshCraftApplication` "god object") is the main
*in-repo* multi-session task, not a bug — research originally found **280
data members + 113 methods** in that one class (11,544 lines across 17
`.cpp` files). As of today (2026-07-18), **7 phases are done**:
`KeybindingManager` (Phase 1), `Preferences` (Phase 2, narrow), and this
session's own `MacroRecorder` (3), `UndoManager` (4), the animation-
override computation (5, narrowed — only `computeAnimOverridesAlg`, not
the ~800-line timeline UI), `WalkController` (6), `AudioPreview` (7). Two
further roadmap candidates were investigated and explicitly **declined**
rather than silently skipped: file dialogs (already well-factored UI glue
over tested `Alg` functions, no testability win) and post-processing
(bloom/skybox/SSAO — 100% side-effecting raw GL calls sharing one fragile
resource bundle with a documented prior bug, no unit-testable logic, real
regression risk with no de-risking tool available). This isn't blocking
anything else in the repo — `MeshCraftApplication` is still a large class
overall, but every remaining piece has now been looked at directly rather
than assumed extractable. Full roadmap + every phase's writeup in
`plan.md`'s `SYS-W3-01` entry.

## 5. Known bugs and limitations

- **Resolved:** a fresh CMake reconfigure of this repo's default
  Release/EasyGL build briefly failed entirely — `../cna`'s `develop` HEAD
  didn't propagate its `CNA_BACKEND_EASYGL` compile definition to this
  repo's own targets. Not caused by this repo; **fixed 2026-07-17 in
  `../cna` commit `730ebbe9`** with explicit user authorization to touch
  the normally-out-of-bounds sibling repo (see §4 for the full writeup).
- **Confirmed, external, not actionable from this repo:** Web/Emscripten
  build crashes inside `../cna` on the first `SDL_EVENT_WINDOW_RESIZED`
  (`GameWindow::queryClientBoundsFromSDL()` calls `SDL_GetWindowSize()`
  after the video subsystem reports uninitialized) — blocks web
  live-verification entirely. Windows/MinGW: 1 remaining compile failure,
  also in `../cna`.
- **Confirmed, by design, deferred:** SVG textures parse/edit but are never
  rasterized; `embed:` mesh references parse/edit but aren't resolved on
  export; scripts/triggers are data-model + editing only, no runtime
  execution (the editor's Audio-tab preview-playback button, added
  `SYS-W3-01` Phase 7, is an editor convenience — it does not mean the
  exported scene has a runtime audio engine); `rotation_units="radians"` /
  non-default `euler_order`, and now also `coordinate_system`
  (`SYS-W14-14`), are honored on export but not in live editor interaction
  (won't-fix, tracked as `STAB-0701`).
- **Resolved (`SYS-W14-15`, 2026-07-18):** native file-browse dialog via
  `CNA::Devices::FileDialog` (material texture slots only — other
  manual-path fields, e.g. Import OBJ, still use text entry). Was
  previously "no native dialog, drag-and-drop only."
- **Resolved:** `Editor::EditorViewport`'s long-open finish-or-delete
  decision (bundled a camera + gizmo + `pickRay()`, added as an explicit
  "stub" in commit `580105d`, never wired in) — **deleted 2026-07-17**
  (see top-of-file decisions block). `camera_`/`gizmo_` remain separate
  `MeshCraftApplication` members.
- **Resolved:** duplicate object IDs are proven safe at the `mc3` library
  level (no crash/data loss) and now surface a warning-level
  `Mc3Validation` diagnostic (decided + implemented 2026-07-17,
  `SYS-W1-04`, now `DONE`); parsing stays permissive.
- **Needs verification:** whether the sibling `mesh-world` repo's R-series
  work (R104+) will touch files this repo also cares about — check
  `git log` at the start of any future session, don't assume `plan.md`
  alone reflects everything that has changed.
- **Risky assumption to watch:** the editor (`SceneRenderer`) and the
  exporter (`mc3togltf/MeshBuilder.cpp`) independently generate geometry
  for every primitive/CSG type from the same `Mc3Document` fields. Nothing
  enforces they agree except a differential test that doesn't cover every
  primitive/case (see §6). A change to one without checking the other can
  silently make "what you see" not match "what you export."

## 6. Architecture notes

- **`Mc3Document`** (`mc3/include/MeshCraft/Mc3/Mc3Document.hpp`) — the
  canonical AST. CNA-free. Public API is depended on by `mc3togltf`,
  `mc3tomcb`, the editor, and every test fixture — **additive changes
  only**; check all four before changing an existing signature.
- **Two independent geometry generators, same source data:**
  `mc3togltf/src/MeshBuilder.cpp::buildPrimitive()` (export + CSG) and
  `SceneRenderer`'s primitive dispatch (`SceneRenderer_Builders.cpp`,
  editor viewport). No shared code. **Triangle winding differs
  deliberately between them** — export is CCW-from-outside (glTF/OpenGL
  convention), the editor preview is CW-from-outside (CNA's default
  `RasterizerState`). Do not "fix" one to match the other; getting it
  backwards renders a see-through mirrored interior, not an obvious crash.
- **CSG dual-path invariant:** `mc3togltf/src/CsgEvaluator.cpp` (export) and
  `SceneRenderer`'s CSG preview cache (editor) both key off `isCutter` /
  `role="cutter"`. Keep both in sync if CSG semantics change.
- **`MeshCraftApplication`** (`include/MeshCraft/MeshCraftApplication.hpp`)
  — the main editor class, historically a "god object": 280 data members +
  113 methods, implementation spread across 17 `.cpp` files by *area* (not
  by *ownership*). Being incrementally decomposed (`SYS-W3-01`, see §4).
  Fifteen subsystems are extracted into owned helper objects with narrow
  interfaces: the 9 pre-existing (`SelectionManager`, `EditorCamera`,
  `TransformGizmo`, `SceneRenderer`, `GridRenderer`, `SceneHierarchyPanel`,
  `PropertiesPanel`, `AiAssistant`, `ModelRegistry`) plus
  `KeybindingManager`, `Preferences`, `MacroRecorder`, `UndoManager`,
  `WalkController`, `AudioPreview`. Two idioms coexist, chosen per
  subsystem's actual entanglement (research the real call-site surface
  first, don't assume): (1) fully self-contained value member with
  whatever it needs passed per-call, no back-reference to the owner
  (`Preferences`/`KeybindingManager`/`UndoManager`/`WalkController`/
  `AudioPreview`); (2) a `PropertiesPanel`-style callback `Context` struct
  built fresh per call site, for logic that genuinely must call back into
  many private `MeshCraftApplication` members (`MacroRecorder`). File
  dialogs and post-processing were investigated as further candidates and
  explicitly declined (see §4/§8) — not every remaining piece is a good
  extraction target.
- **`Alg` mirror pattern:** pure-logic, CNA-free free functions
  (`include/MeshCraft/EditorAlgorithms.hpp`,
  `src/MeshCraft/AiResponseAlgorithms.hpp`) mirror some production code
  paths so they're headlessly testable. Most are the real production
  implementation (the `.cpp` calls into them); a few are deliberately-kept
  duplicates. At least two have already drifted from production despite
  the "kept in sync" intent (`insertAnimKeyframesAlg`/`loadPrefsAlg`,
  `AUD-030` fixed the first, `AUD-031`/`032`/`033` cover the audit of the
  rest). **Check whether an `Alg` function is actually called from
  production before assuming a fix there takes effect.**
- **Undo/redo:** whole-document snapshot-based (deep copy on every
  mutating command), not command/diff-based. The stacks now live in
  `Editor::UndoManager` (`SYS-W3-01` Phase 4) — `MeshCraftApplication`'s
  `pushUndo()`/`performUndo()`/`performRedo()` are thin wrappers that
  prepare an independent `deepCopyDoc()` copy and hand it in; the manager
  itself has no dependency on `deepCopyDoc()` or any CNA type.
- **`mc3.xsd` is compiled into the binary at configure time** — editing it
  requires a reconfigure, not just a rebuild.
- **XML comment gotcha:** a literal `--` inside an XML comment is rejected
  by `lxml`/`test/validate_xsd.py`, though `tinyxml2` tolerates it
  silently. Run `test/validate_xsd.py` on any new/edited `.mc3.xml`
  fixture before trusting it.
- **`AiAssistant` threading invariant:** background HTTP runs on a
  detached `std::thread` writing into a `shared_ptr<AiRequestResult>`
  (atomic done flag + mutex). Never switch this to `std::async`/
  `std::future` (reintroduces a destructor-blocking hang-on-close that was
  already fixed once).
- **`CNA_ENABLE_NET` must stay `OFF`** — an unused CNA subsystem that fails
  to compile; re-enabling breaks the default build.
- **Boundaries that must not be broken:** no changes to `../cna` or
  `../sharp-runtime` without owner permission (a separate process handles
  them). No `${meta-gl_SOURCE_DIR}/include` in `CMakeLists.txt` (triggers a
  full CNA recompile). Commits from the sibling `mesh-world` repo's
  R-series backlog can land on this repo's `develop` independent of this
  repo's own `plan.md` — don't assume `git log` only contains commits this
  repo's own backlog asked for.

## 7. Useful commands

```bash
# Configure + build (Release, EasyGL backend — verified clean on a fresh
# reconfigure as of ../cna commit 730ebbe9, see §4).
cmake -S . -B b-release
cmake --build b-release -j"$(nproc)"

# Full test suite
(cd b-release && ctest -j"$(nproc)")
(cd b-release && ctest -N)                              # list registered tests + live count
(cd b-release && ctest -R field_matrix --output-on-failure)   # SYS-W6-04's gate, now green

# Plan/doc self-consistency (run before trusting any count in plan.md/NEXT.md)
python3 test/validate_plan_consistency.py . b-release

# XSD validation of a new/changed fixture
python3 test/validate_xsd.py mc3/mc3.xsd test/some_fixture.mc3.xml

# Lint/format (config checked in this session; clang-format needs
# `pip install clang-format` first if not already on PATH)
clang-format -i path/to/changed/file.cpp
cmake -S . -B b-release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p b-release path/to/changed/file.cpp

# Run / demo
./b-release/MeshCraft test/house.mc3.xml
./b-release/MeshCraft test/house.mc3.xml --screenshot /tmp/out.png   # real PNG now (SYS-W14-03); .ppm still raw PPM
./b-release/MeshCraft test/house.mc3.xml --benchmark                # SYS-W12-02: in-process timing, no GUI needed
./b-release/mc3togltf/mc3togltf test/features.mc3.xml /tmp/out.glb --stats
./b-release/mc3tomcb/mc3tomcb test/house.mc3.xml /tmp/out.mcb
```

## 8. Next smallest tasks

_(Everything from the previous revision of this list is done — see §3 for
today's full list. Before starting any task below, re-run
`git log --oneline -20` and re-check the cited `plan.md` row's status
yourself — this file has repeatedly found stale `[TODO]` markers across
sessions; don't assume anything below is still accurate without looking.
Re-run `python3 test/validate_plan_consistency.py . <build-dir>` too, it
recomputes every count live.)_

**Everything actionable is done.** What's left in `plan.md` is either
owner-gated/externally blocked, or a `SYS-W3-01` candidate already
investigated and explicitly declined this session (not silently skipped —
see §4 and `plan.md`'s `SYS-W3-01` entry for the reasoning):

1. **CI enablement — needs the repo owner, not more investigation:**
   `AUD-052`/`SYS-W11-01` (un-park `.github_/workflows/ci.yml` →
   `.github/` — needs a `workflow`-scoped push token nobody in this
   environment has), which is the hard blocker for `AUD-053`/`SYS-W11-03`
   (editor build+test CI job) and the CI-job half of `AUD-057` (sibling-
   repo revision pinning — its configure-time-assertion half already
   landed, commit `d2943e3`). Nothing to do here without that token; don't
   re-investigate, just wait for owner action.
2. **`AUD-042`** (P2) — Android build forces `SDL_RENDERER`. Blocked: no
   Android NDK in this environment, and it intersects CNA backend
   behavior (out of scope without owner permission per `CLAUDE.md`).
3. **`AUD-025`/`AUD-038`** (DEFERRED) — informational findings
   (`embed:` mesh handling, the 20-entry undo cap), not gaps to close.
4. **If `SYS-W3-01` is ever revisited:** the two declined candidates
   (file dialogs, post-processing) were investigated with real reasoning,
   not skipped for lack of time — re-read `plan.md`'s `SYS-W3-01` entry
   before assuming either is still worth a fresh look. Post-processing in
   particular would need a dedicated visual-regression harness (real
   before/after pixel comparison against an emissive-material scene)
   built *first* if it's ever attempted, given its documented fragility
   history (`project_bloom_bug` memory).
5. **`AUD-036c`'s one intentionally-accepted gap:** "locked objects
   untouched by undo/redo" is well-tested at the per-command level but not
   independently tested as its own whole-document-snapshot guarantee (see
   `AUD-036c`'s status note in `docs/history/plan_20260718.md` — the
   sibling item, selection restore, was closed by `SYS-W9-03`). By design,
   not a real gap; lowest priority of anything listed here.

If none of the above is actionable (the common case right now), the next
useful move is a **repo-wide staleness spot-check** — this file's own
history shows real value in that (multiple sessions have each found a
handful of stale `[TODO]`/status markers) — or asking the user directly
what they'd like worked on next, since the backlog itself is essentially
exhausted of unblocked work.

## 9. Do not do yet

- **No broad `MeshCraftApplication` refactor in one pass.** `SYS-W3-01` is
  explicitly phased (research this session sized it at 280 members/113
  methods); do one subsystem at a time, verify, commit.
- **No mass `clang-format -i` across the existing 18.5k LOC.** The config
  added this session (`.clang-format`) was deliberately not applied
  tree-wide — that's a separate, much larger, not-yet-decided change.
- **No changes to `../cna` or `../sharp-runtime`** without explicit owner
  permission.
- **No `Mc3Document` public API changes** without checking `mc3togltf`,
  `mc3tomcb`, the editor, and all test fixtures first — additive only.
- **No attempt to unpark CI** (`.github_/workflows/ci.yml` → `.github/`) —
  owner-gated, needs a workflow-scoped push token nobody in this session
  has.
- **Ask before assuming a new feature is in scope**, per `CLAUDE.md`'s
  plan.md workflow. `STABILIZATION.md`'s original "no new features until
  gates are green" policy was the project's default for a long time; it's
  now archived (`docs/history/STABILIZATION.md`, moved 2026-07-18) since
  the stabilization phase is substantively complete (gates A-E satisfied)
  and the user has repeatedly, explicitly authorized batches of new
  features in practice (the `SYS-W14-##` rows, including `-03`/`-08`
  today) — treat that as a standing willingness to grant scoped
  exceptions on request, not a blanket "anything goes now."

## 10. Resume prompt

```
Read NEXT.md first, in full. Then work on exactly ONE task from its
"Next smallest tasks" section — start with task 1 unless told otherwise.
Inspect only the files that task names; do not refactor or "clean up"
anything else you notice along the way. Make one small, verified
improvement: implement it, then run the exact verification command the
task lists (and the full `ctest` suite) before considering it done. Do
not start a second task in the same session unless the first is fully
committed and verified. When finished, update NEXT.md: move the completed
task out of "Next smallest tasks", update "Current status"/"Recent
changes" with what actually changed (not what was planned), and re-check
every other section for anything your change made stale.
```
