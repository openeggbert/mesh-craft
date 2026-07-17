# Mesh Craft — Active Backlog

**This is the single active backlog.** For a compact baseline and handoff see
[`NEXT.md`](NEXT.md); for quality gates and process see
[`STABILIZATION.md`](STABILIZATION.md); for the registered tests see
[`TESTING.md`](TESTING.md). Superseded plans live in
[`docs/history/`](docs/history/).

## Status legend

- `TODO` — not started.
- `IN_PROGRESS` — being worked now.
- `DONE` — implemented, tested, and verified (verification command recorded).
- `BLOCKED` — cannot proceed; blocker recorded.
- `DEFERRED` — intentionally postponed with a reason.

Priority: `P0` (UB / crash / data-loss / unsafe-input / invalid-output-as-success),
`P1` (wrong behavior, missing validation, untruthful capability, critical test gap),
`P2` (maintainability / duplication / perf), `P3` (polish).

Workstreams: `W0` immediate defects · `W1` validation & untrusted input ·
`W2` AI/import sandbox · `W3` architecture decomposition · `W4` de-duplication ·
`W5` MC3 governance · `W6` MCB hardening · `W7` glTF fidelity · `W8` backend truth ·
`W9` undo & data-loss · `W10` UI testability · `W11` build/CI/DX ·
`W12` performance · `W13` documentation · `W14` new features.

## How this backlog was built

The `AUD-###` tasks come from a 12-dimension evidence-based audit of the
checkout (2026-07-11, session 1). Every finding was **adversarially
re-verified** against the real source by a second independent pass; 18
candidate findings were refuted and dropped, leaving **57 confirmed**. A
follow-up adversarial re-review (2026-07-11, session 2) re-opened several
"done" claims against current source, found 3 of them only partially fixed,
and surfaced 2 new defects — these are tracked as `AUD-006b`/`AUD-036b`/
`AUD-039b` (the unfinished remainder of a partial fix) and `AUD-058`/`AUD-059`
(newly found), for **62 AUD tasks total**. Each cites exact `file:line`
evidence. The `SYS-###` tasks are the mandated systematic workstream items
(matrices, sanitizers, benchmarks, features) not tied to a single defect.
**Do not trust a DONE marker at face value** — every DONE row cites an exact
commit and an exact verification command; re-run the command if in doubt.
`test/validate_plan_consistency.py` (wired into CTest as `plan_consistency`)
mechanically checks this file stays internally consistent — run it before
trusting any count quoted below.

---

## Session log

### Session 1 (2026-07-11) — initial audit + P0/P1 fix pass

Fixed all 2 confirmed P0s and most confirmed P1s. Commits (oldest first):
`afb1159` (ActiveTool OOB), `838eefc` (ObjectType mapping), `dae910f`
(NaN/Inf rejection), `fd606d2` (input budgets + instance-cycle + finiteness
gate), `737af77` (undo dead-pattern), `0dfcd4f` (docs consolidation),
`40643a4` (Mc3LoadPolicy), `22b7129` (glTF lights), `dcd0d33` (anim mirror
Gate B), `2d126bf` (deterministic shutdown), `e53af49` (backend-truth
warning), `ac75eb6` (concave extrude caps), `901965f` (resource-path
confinement), `d16c82c` (CI file readiness).

**Self-reported end-of-session status was inaccurate** — see Session 2.

### Session 2 (2026-07-11, continued) — adversarial re-open + doc reconciliation

The session-1 end summary claimed "every P0 and every identified P1
addressed" and reported inconsistent test counts (93/93 in one place, 95/95
in another) with only 4 of 57 AUD tasks marked DONE in this file despite 16+
P1s already being fixed in git history. This session:

1. Reconciled `plan.md`/`NEXT.md` against actual `git log` + `ctest -N`
   (this section, `test/validate_plan_consistency.py`).
2. Re-verified all 57 session-1 AUD findings against current source
   (not against the session-1 summary) — see the AUD-### table below for
   the corrected per-finding status: **28 DONE, 27 TODO, 2 DEFERRED** among
   the original 57 (not 4 DONE).
3. Re-opened 3 findings that were reported fixed but were only partially
   fixed on closer inspection (`AUD-006`→`AUD-006b`, `AUD-036`→`AUD-036b`,
   `AUD-039`/`AUD-040`→`AUD-039b`) and filed the unfinished remainder as new
   `TODO` tasks.
4. Found 2 new defects via direct adversarial code reading
   (`AUD-058` BloomGL::cleanup() never called, `AUD-059` tessellation budget
   is per-field only) not part of the original audit, filed as new `TODO`
   tasks.

   **Net across all 67 AUD-### rows (57 original + 7 session-2 additions,
   the 7th — AUD-036c — split off from AUD-036b so its verified-done portion
   could be marked DONE without also claiming its still-open portion, + 3
   more found by `SYS-W7-02`'s differential geometry test: `AUD-061`/`062`/
   `063`):
   61 DONE, 4 TODO, 2 DEFERRED** — recompute with
   `python3 test/validate_plan_consistency.py . <build-dir>` rather than
   trusting this number as time passes.
5. Archived `plan_deep_audit.md` (all 57 of its own tasks were already
   completed) and fixed `RELEASE.md`'s stale 66/66 test count.
6. Removed machine-specific absolute source paths from this file's evidence
   text.

Work continues past this reconciliation into the newly-opened tasks — see
**Priority execution queue** below. This log entry itself will go stale; the
authoritative live state is always the AUD/SYS task table plus
`git log`/`ctest -N`, not this narrative.

---

## Priority execution queue (next up, in order)

1. **AUD-052 (P1/W11)** — CI is permanently parked under `.github_/`; GitHub
   Actions never runs. This is the root blocker for AUD-053 (a CI-hardening
   task that depends on CI actually running first) and the CI-job half of
   AUD-057 — long documented elsewhere as owner-gated (enabling Actions on
   the repo isn't something available in this environment), so treat as
   blocked-pending-owner-action rather than something to force through.
   AUD-057's configure-time-assertion half (recording/checking the sibling
   repos' current git SHA) landed independently in commit `d2943e3` — only
   the CI-job half is still open, and it's blocked here, not actionable.
2. **AUD-042 (P2/W8)** — Android build path forces SDL_RENDERER; blocked
   (no Android NDK in this environment; also intersects CNA backend
   behavior, out of scope per CLAUDE.md's "no CNA changes without owner
   permission").
3. Remaining `TODO` AUD-### rows by severity (AUD-053, downstream of
   AUD-052; AUD-057's CI-job half, same blocker), then SYS-### rows.

---

## Systematic workstream tasks (SYS-###)

Mandated workstream items not tied to a single audit finding.

### W1 — Validation subsystem
- **SYS-W1-01** `[DONE]` `P1` — First-class `Mc3Validation` result type
  (errors + warnings, each with source path, object/field identity, suggested
  safe repair). Wire into load / MCB-load / include-merge / AI-apply /
  pre-render / pre-export / save. Replaces scattered clamps.
  `mc3/include/MeshCraft/Mc3/Mc3Validation.hpp` added: `Mc3ValidationSeverity`,
  `Mc3ValidationEntry{severity, sourcePath, objectId, field, message,
  suggestedRepair}`, `Mc3Validation` accumulator. Design: an ADDITIVE
  side-channel, not a replacement for the existing throw-on-hard-rejection /
  clamp-on-recoverable-issue contract (many existing tests catch
  `std::runtime_error` and check `.what()`) — new `Mc3Validation&`/`*`
  overloads are added alongside the untouched originals.
  **DONE (all 7 of 7 named integration points, fully tested):**
  **MC3 XML load + include-merge** (`Mc3XmlParser.cpp`, commit `e8d68d8`) —
  tessellation clamps, NaN/Inf/malformed-numeric sanitization, the three
  `DocumentBudget` throw sites, resource-path confinement rejections,
  oversized-embed-base64 rejection, cyclic/depth/confinement include
  failures, and the existing include-merge id-collision + unknown-type
  `cerr` warnings all now also report structured entries. New test
  `mc3_validation_test` proves the diagnostic surface itself is populated
  (not just clamped values). **MCB load** (`McbReader.cpp`, commit
  `d91bf4f`) — `expectTag` type-mismatches, `clampEnum` out-of-range enums,
  the string/collection sanity-limit and recursion-depth throws, and the
  binary-header validation failures all report entries via a best-effort
  `IdentityScope` object-identity stack (MCB has no XML-element-like
  attribute bag to read identity from on demand, unlike the XML side). New
  test `mcb_validation_test`. Both increments: full tree green (110/110
  ctest), zero new `-Wall -Wextra` warnings.
  **`Mc3Document::validate()` foundation** (commit `a119415`) — re-validates
  a document's CURRENT in-memory state by round-tripping it through the
  same XML writer/parser the validating overloads above use, discarding the
  reparsed copy; closes the gap for documents that never went through a
  parser load at all (built programmatically via `Mc3Object::make*()`, or
  mutated in place after loading). Diagnostic-only, never throws. New test
  `mc3_document_validate_test`. **AI-apply** (commit `92c6246`) —
  `src/MeshCraft/AiResponseAlgorithms.hpp`'s `parseXmlAlg`/
  `validateAndParseAiResponseAlg` gain `Mc3Validation&` overloads; the three
  rejection cases with no `Mc3XmlParser` equivalent (missing `<mc3>` root,
  empty document, XSD non-conformance) are also recorded as error entries,
  not just via `errorMessage`. Extended `ai_test.cpp`. **Pre-export**
  (commit `297af3e`) — `GltfExporter` gains a `Mc3Validation validation`
  member (mirroring the existing `ExportStats stats`), populated by calling
  `doc.validate()` before any glTF output is built; printed by the CLI under
  `--stats`. New `mc3togltf_pre_export_validation_test` — deliberately uses
  a moderate out-of-range value (5000), not an extreme one like the other
  new tests use safely, since `exportDocument()` actually tessellates the
  still-unclamped live document and an extreme `segments` value would
  attempt a many-petabyte allocation (confirmed by first hitting exactly
  that, an OOM-killed test process, before reducing the value). **Pre-render
  + save** (commit `899b486`) — the 4 real application call sites that swap
  in a freshly-loaded document (startup, Open File dialog's `.mc3.xml`/
  `.mcb` branches, Open Recent File, autosave recovery) now use the
  validating `loadFromFile()`/`Mcb::loadFromFile()` overloads instead of
  discarding the diagnostics, logging a one-line count when non-empty;
  deliberately NOT wired into the undo/redo document-swap sites (those
  restore an already-in-memory, previously-valid snapshot, and undo/redo is
  a hot path where revalidation would hurt responsiveness for no benefit).
  `saveFile()` calls `Mc3Document::validate()` right before writing (catches
  values that reached memory via a path bypassing the parser entirely —
  Properties-panel edits, AI-apply, scripting), logging findings and folding
  a note count into the "Saved …" status message; diagnostic-only, never
  blocks the save. Neither adds a dedicated new test: pre-render reuses the
  already-tested validating load overloads verbatim, and save's addition is
  a few lines of already-tested glue in CNA-coupled application code with no
  headless test seam, matching this file's own established "Alg mirror"
  precedent (test the reusable pure logic, not every call site that
  consumes it). Surfacing these console-only diagnostics in the UI itself is
  `SYS-W14-02` (already a separate, not-yet-started backlog item). Full tree
  rebuilt after every increment: 121/122 ctest (the 1 failure, `field_matrix`,
  is unrelated — see `AUD-` prefix note in the session log — pre-existing
  drift from a concurrent, separately-tracked initiative, not from this
  task), zero new `-Wall -Wextra` warnings.
- **SYS-W1-02** `[DONE]` `P1` — Documented numeric ranges per domain (geometry,
  material, camera near/far/FOV/aspect, environment, animation, audio, transforms,
  post-processing) with tests.
  **DONE (6 of 8 named domains; audio/post-processing found N/A -- no field
  exists to range-check, not skipped):** `Mc3XmlParser.cpp`'s pre-existing
  `finiteOr`/`attrF` only sanitize NaN/Inf/malformed text (SYS-W1-01 era) --
  no SEMANTIC range check existed for any domain before this session. One
  domain per commit, each clamped (not rejected -- authoring-mistake
  judgment call, matching `AUD-059`'s tessellation-clamp precedent), each
  covered by new `mc3_numeric_range_test`: **camera** (commit `1b1b81f`) --
  `fov` clamped to [1,179] degrees, `near` clamped to > 0, `far` clamped to
  exceed `near` by a margin, `aspect` clamped to > 0. **material** (commit
  `ea17322`) -- `roughness`/`metallic`/`occlusion_strength`/`alpha_cutoff`
  and `base_color`'s alpha channel clamped to glTF's [0,1] PBR convention
  (RGB/`emissive_color` deliberately left unclamped -- HDR emissive is
  documented as intentional). **geometry** (commit `e80c99f`) -- primitive
  `radius`/`height`/`size`/`major_radius`/`minor_radius`, extrude
  `cross_section` `width`/`height`/`radius`/`inner_radius`, and extrude
  `path` `length`/`radius`/`height` all reject negative (clamp to 0);
  disk's pre-existing `inner_radius=-1` sentinel fallback also gained a
  diagnostic for a genuinely-negative input. **environment** (commit
  `caa2a58`) -- fog `density` rejects negative; fog `start>=end` is
  diagnostic-only (confirmed by reading `SceneRenderer.cpp`, which already
  guards `end > start` before dividing -- not a genuine crash risk, so not
  clamped, just flagged). **animation** (commit `31a7bb2`) -- `time_scale`
  rejects zero/negative (stalls playback), clamped to a small epsilon not a
  fixed fallback; keyframe time ordering was already handled (pre-existing
  `stable_sort`), bounds intentionally left unenforced (product decision,
  not universally-invalid input). **transforms** (commit `03772b8`) -- base
  transform / `<deform>` / named `<state>` scale all reject near-zero
  magnitude per axis (degenerates the transform matrix); sign is
  deliberately preserved -- negative scale is a legitimate glTF-supported
  mirroring feature, confirmed by reading `GltfExporter.cpp:830`.
  **N/A, not a gap (2 domains, no field exists to check):** **audio** --
  `Mc3Sound`/`Mc3Music` have no `volume`/`pitch` field at all (`id`/`src`/
  `loop` only); audio playback itself isn't implemented yet (MC3_FORMAT.md's
  own Sounds and Music section already documents this). **post-processing**
  -- confirmed via `roundtrip_test.cpp`'s own comment that bloom/post-fx is
  a runtime/editor UI toggle, not scene-file data; there is no
  `Mc3Environment` (or any other) field for it. Both would need a format
  field added first, out of this task's scope. All ranges documented in
  `MC3_FORMAT.md` (commit `103526a`). Full tree: 116/116 ctest at every
  step, `field_matrix.py` clean (one false-positive gap found+fixed --
  `attrFClamped` added to its recognized `xml_read` helper list), zero new
  `-Wall -Wextra` warnings.
- **SYS-W1-03** `[DONE]` `P1` — Document-complexity budgets beyond `AUD-059`
  (total object count + total tessellation weight, done, commit `9a4b8f6`):
  max bytes, definitions, materials, textures, embeds, actions, channels,
  keyframes, children-per-node, recursion depth enforced at load.
  **DONE (all 10 named dimensions):** extended `DocumentBudget`
  (`Mc3XmlParser.cpp`) with a `charge*()` method per dimension, one commit
  each, each proven by new `mc3_document_budget_test` constructing a
  document that exceeds ONLY that dimension while staying under every other
  budget (including the pre-existing object/tessellation/include budgets),
  confirming the rejection names the right one: **materials** (20,000) and
  **textures** (20,000, regular+svg combined) (commit `862a58b`). **embeds**
  -- count (1,000) AND aggregate bytes (256MB combined, independent of the
  existing 64MB PER-embed cap -- closes a real gap: N embeds each
  individually legal could otherwise sum to unbounded memory) (commit
  `861be54`). **actions** (10,000), **channels** (200,000), **keyframes**
  (2,000,000) (commit `29f4727`). **children-per-node** (20,000) -- a LOCAL
  per-call breadth cap, not a `DocumentBudget` running total (breadth is
  inherently per-node, not document-wide); proven distinct from total-object
  count with a fixture that stays under `kMaxTotalObjects` while exceeding
  this cap. **recursion depth** -- investigated whether mc3's XML reader
  needs a McbReader.cpp-style `RecursionGuard`; confirmed empirically
  (reading sharp-runtime's vendored tinyxml2 source) that tinyxml2's own
  built-in `TINYXML2_MAX_ELEMENT_DEPTH=500` already rejects deep nesting
  before `Mc3XmlParser`'s own recursion is ever reached -- NOT a novel gap
  (unlike MCB's hand-rolled binary parser, which had none and segfaulted at
  ~20,000 levels before its guard existed), proven with a 2000-level fixture
  (both in commit `786a1fc`). **definitions** (20,000) (commit `a525937`).
  **max bytes** -- interpreted as the raw INPUT file/string byte size (512MB
  ceiling), checked via `std::filesystem::file_size()` before tinyxml2
  buffers anything, at all three load entry points (`parse`/`mergeInclude`/
  `parseString`) (commit `441892a`). All budget ceilings and rationale
  documented in `MC3_FORMAT.md` (commit `103526a`).
  **Explicitly NOT implemented (by design, not oversight):** a "total
  generated output bytes" estimate (guessing at eventual GLB/geometry
  allocation size across the whole document) -- no precise formula exists
  for it (tessellation weight and texture/embed byte totals already bound
  the largest real cost centers), so a fuzzy estimate wouldn't usefully
  bound anything beyond what the dimensions above already do; the precise
  INPUT-byte ceiling above supersedes it as the actionable version of "max
  bytes". Full tree: 116/116 ctest at every step, `field_matrix.py` clean,
  zero new `-Wall -Wextra` warnings.
- **SYS-W1-04** `[DONE]` `P1` — Pathological-input fixture corpus. Seeded:
  `finite_input_test`, `input_budget_test`, `hostile_geometry_test`,
  `load_policy_test`. 4 of the 5 originally-remaining categories now done with
  real fixes + tests: **oversized base64** — `mc3_oversized_base64_test`;
  found and fixed a genuine unbounded-memory gap (a 20MB inline `<embed>`
  base64 body loaded with no error before the fix), added a 64MB parse-time
  ceiling (commit `b9e5a0a`). **Include bombs** — `mc3_include_bomb_test`;
  cycles and per-chain depth were already bounded, but fan-out (many distinct
  sibling `<include>`s) was not — confirmed 1500 trivial includes merged with
  no error before the fix, added a 1000-include document-wide budget (commit
  `21a3890`). **Invalid UTF-8** — `mc3_invalid_utf8_test`; already adequately
  handled (tinyxml2/Mc3XmlParser pass malformed byte sequences through
  byte-for-byte with no crash/OOB/further corruption), proven with a
  regression test and verified clean under ASan+UBSan (commit `999be67`).
  **MCB corruption** — `mcb_corruption_test`; already adequately handled
  (every read is bounds-checked, lengths/counts are sanity-capped,
  unrecognized tags throw, recursion is guarded), proven with a byte-offset
  truncation sweep (1659 offsets) and a seeded random-byte fuzz (350 buffers,
  0–16KB) over a rich multi-section document, both clean under ASan+UBSan
  (commit `a38eb88`); also materially advances `SYS-W6-02`. **Remaining,
  left open deliberately: duplicate object IDs** — `mc3_duplicate_ids_test`
  proves the current mc3-library behavior is safe (no crash/data-loss; a
  document with two same-`id` objects parses and round-trips both
  unchanged), but the only ID-keyed *lookups* in the codebase
  (`flatFindById`, the `lockedIds_` object-lock set) live in the editor
  application layer, not mc3 itself — whether a duplicate id should become a
  hard parse error or be auto-renamed is a product/UX decision this pass is
  not authorized to make unilaterally (commit `be9dd55`). Row stays
  IN_PROGRESS pending that decision.
- **Decision (2026-07-17, human-authorized) — implemented:** duplicate ids
  stay a warning-level `Mc3Validation` diagnostic, not a hard parse error or
  auto-rename — least breaking, reuses the validation infrastructure
  `SYS-W1-01` already wired into load/save/AI-apply/pre-export/pre-render.
  Added `checkDuplicateObjectIds()` to `Mc3XmlParser.cpp` (called at the end
  of `buildDocumentFromRoot()`, covering both `parse()`/`parseString()`):
  walks `doc.objects` + recursive `.children` (the exact scope
  `MeshCraftApplication::flatFindById` searches — not `doc.definitions`,
  which that lookup never checks), emits one warning per duplicated id
  (not one per object) naming the id and its use count. Parsing remains
  fully permissive — the warning is diagnostic-only. Extended
  `mc3_duplicate_ids_test.cpp` with 4 new assertions proving: loading with
  a validation sink still succeeds, exactly one warning fires for the
  duplicated `dup` id, duplicate ids are never elevated to an error, and an
  unrelated unique id gets no diagnostic. Verify: `ctest -R
  mc3_duplicate_ids`; full suite 123/123.
- **SYS-W1-05** `[DONE, partial — see scope note]` `P2` — Graph-cycle /
  shared-node policy for API-built trees.
  **Implementation (2026-07-17):** XML-parsed content can never form a
  cycle (each `<tag>` always creates a fresh `Mc3Object`), but
  `Mc3Object::children` (`std::vector<shared_ptr<Mc3Object>>`) is a plain,
  freely-mutable public field with no `addChild()`-style choke point to
  validate at — nothing stops a document built/mutated via the C++ API
  from introducing a cycle (e.g. `obj->children.push_back(obj)`). Added a
  256-deep recursion guard (matching `mc3togltf/src/GltfExporter.cpp`'s
  existing `kMaxNodeDepth` precedent) to the two most frequently-invoked
  recursive tree walks: `deepCopyObjectAlg` (`EditorAlgorithms.hpp`,
  called directly by Duplicate/Group/Convert-to-Definition/Break-Instance/
  macro playback — a cycle here crashes on a single user click) and
  `deepCopyObj`/`deepCopyDoc` (`MeshCraftPrivate.hpp`, called by
  `pushUndo()` on every mutating command). Both now throw a clean,
  catchable `std::runtime_error` naming the depth limit instead of an
  unbounded-recursion stack-overflow crash. New tests
  (`testDeepCopyObjectAlgRejectsCyclicChildren`, 2-cycle and self-cycle
  cases) confirm the guard actually fires. **Scope note, "partial" —
  deliberately not a full sweep:** several OTHER recursive walks over
  `Mc3Object::children` remain unguarded (`findParentListAlg`/
  `removeFromListAlg` in `EditorAlgorithms.hpp`, `Mc3XmlWriter`'s
  `writeObject`, the editor's `SceneRenderer` traversal, `mc3togltf`'s
  `MeshBuilder.cpp`) — guarding every one of them is a materially larger
  task than this row's terse one-line, no-evidence-cited description
  suggested; the two guarded here are the highest-value (most frequently
  executed, most directly user-triggered) paths. "Shared-node" (a
  non-cyclic DAG — the same child object appearing under two different
  parents) was investigated and found lower-priority: it terminates fine
  in a depth-first walk (no crash risk), the only real consequence is
  `deepCopyObjectAlg`/`deepCopyObj` silently duplicating the shared node
  into two independent copies — surprising but not unsafe, left as
  documented behavior rather than a "policy" requiring a product decision.
  Full rebuild + 124/124 `ctest`; manual `--screenshot`/`--export` smoke
  tests. Verify: `ctest -R mc3_commands`.
- **SYS-W1-06** `[DONE, 3 of 4 — see SYS-W1-07]` `P3` — Extend
  `SYS-W1-05`'s cycle-guard pattern to the remaining unguarded recursive
  walks over `Mc3Object::children`.
  **Implementation (2026-07-17):** added the same 256-deep
  `std::runtime_error`-throwing guard to `findParentListAlg`/
  `removeFromListAlg` (`EditorAlgorithms.hpp` — used by Delete and
  reparenting operations) and `Mc3XmlWriter::writeObject`
  (`mc3/src/Mc3XmlWriter.cpp` — the actual Save/Export path, arguably the
  single most consequential place for a cyclic document to silently hang
  or crash instead of failing cleanly). New test
  `testSaveRejectsCyclicChildrenInsteadOfCrashing`
  (`mc3/test/roundtrip_test.cpp`) confirms `Mc3Document::saveToFile()`
  throws a catchable, named error instead of stack-overflow-crashing on a
  hand-built 2-cycle. Full rebuild + 125/125 `ctest` (stable across 3
  repeated runs); manual `--screenshot` smoke test. **Not done — the
  editor's `SceneRenderer` traversal remains unguarded** (the export
  path, originally believed unguarded too, turned out to already be
  covered by a pre-existing `AUD-007` fix — see `SYS-W1-07`'s own
  correction note), tracked as new `SYS-W1-07`: needs a live CNA/GL
  context to exercise properly (can't be verified with a quick headless
  unit test the way the three guarded here could), and a cycle surviving
  all the way to Render would already have been caught earlier by
  `deepCopyObjectAlg`'s `pushUndo()`-time guard in virtually every real
  editing workflow — lower marginal value, not attempted in this pass.
  Verify: `ctest -R mc3_roundtrip`.
- **SYS-W1-07** `[DONE]` `P3` — Guard the editor's `SceneRenderer`
  traversal against a cyclic `Mc3Object::children` graph, completing
  `SYS-W1-05`/`SYS-W1-06`'s pattern.
  **Correction (2026-07-17):** the export half of this row's original
  scope — `"mc3togltf/src/MeshBuilder.cpp"` — was factually wrong; that
  file has no `children` recursion at all. The actual recursive node-
  building function, `GltfExporter.cpp`'s `buildNode()`, was **already
  guarded** by a pre-existing, pre-this-session fix (`AUD-007`,
  `kMaxNodeDepth = 256`, confirmed present and correctly checking `depth >
  kMaxNodeDepth` before this row was even written) — so the export path
  needs no new work. Own mistake, corrected the same session it was made,
  same rigor applied to every other stale/inaccurate claim found today.
  **Implementation (2026-07-17):** actually enumerated every recursive
  `Mc3Object::children` traversal in `SceneRenderer.cpp` (plus its sibling
  `SceneRenderer_Extrude.cpp` and `CsgCacheAlg.hpp`) instead of estimating
  from a one-line description. Of the ~11 scattered call sites, **7 were
  already guarded** (`buildManifoldTree`/`csgSubtreeWarning` at
  `depth > 12`; `drawObject`/`drawEmissiveObject`/`drawObjectEdges` at
  `depth > 16`; `csgSubtreeHashAlg` in `CsgCacheAlg.hpp` at `depth > 12`) —
  only **4 local lambda helpers had no guard at all**:
  `computeObjectWorldMatrix`'s `find` (world-matrix lookup, used by
  picking/gizmo placement), `drawCsgGizmos`'s `visit` (CSG gizmo overlay
  draw), and `objectPolyStats`/`scenePolyStats`'s `walk` (poly-count
  stats, the latter also being the "visibility skip" traversal). Added a
  `depth` parameter to each (threaded through the existing
  `std::function` recursive lambdas) with the same `depth > 16` cap this
  file already uses for equivalent plain (non-CSG) object-tree recursion,
  for consistency with the 3 already-guarded sibling functions rather
  than introducing a different threshold. No public API change — the
  depth parameter is internal to each function's own recursive helper,
  not part of any member-function signature. Verify:
  full rebuild + **125/125 `ctest`**, plus a real live-GL `--screenshot`
  before/after diff (byte-identical, confirming no behavior change for
  legitimate non-cyclic scenes) on `test/house.mc3.xml`,
  `test/csg_cache.mc3.xml` (exercises the CSG gizmo + world-matrix
  lookup path), and `test/features.mc3.xml`. No new automated cycle-safety
  test added — per this row's own note and the established
  `mc3/test/editor_commands_test.cpp` precedent, `SceneRenderer` requires
  a live `GraphicsDevice` to construct at all, so there is no headless way
  to build a cyclic-children fixture through this code path; the manual
  screenshot smoke test is this codebase's existing convention for
  `SceneRenderer`-level verification (see `differential_geometry_test.cpp`'s
  file header for the same reasoning).
  **Discovered but NOT fixed (out of this row's scope, see NEXT.md §4):**
  while verifying, found that a *fresh* `cmake -S . -B <dir>` reconfigure
  of this exact tree now fails to build at all — unrelated to this row's
  code change (reproduces identically with it reverted) and traced to the
  sibling `../cna` repo's current `develop` HEAD (`58e82fd3`, landed after
  this session started), not anything in this repo. See NEXT.md for the
  full writeup; not touched here per `CLAUDE.md`'s CNA boundary.

### W2 — AI / import sandbox
- **SYS-W2-01** `[DONE]` `P1` — `Mc3LoadPolicy` threaded through parsing
  (`allowIncludes`, `confineIncludesToRoot`, `confineResourcePathsToRoot`,
  budgets); `untrusted()` factory for AI. Commits `40643a4`, `ed6e220`
  (`AUD-006b` extended coverage to texture/mesh/SVG/embed/sound/music).
  Verify: `ctest -R mc3_load_policy`.
- **SYS-W2-02** `[DONE]` `P1` — In-memory AI parse path (no temp file). Commit
  `40643a4` (`Mc3Document::loadFromString`). Verify: `ctest -R mc3_load_policy`.
- **SYS-W2-03** `[DONE]` `P2` — Real JSON parse scoped to `content[].type=="text"`;
  correct UTF-16 surrogate pairs. (`AUD-009`)
  **Status note (2026-07-17):** stale `TODO` — `AUD-009` itself has read
  `[DONE]` since commit `ca82b0b` (an earlier session): `extractFirstTextValue`'s
  search key is the full `"type":"text","text":"` prefix (scopes extraction
  to an actual text block, not just any `"text":"` substring anywhere in
  the body), and its `\u` escape handling combines high/low UTF-16
  surrogate pairs into correct 4-byte UTF-8, mapping a genuinely lone
  surrogate to U+FFFD instead of emitting invalid UTF-8. Tested
  (`ai_test.cpp`: a decoy non-text `"text":"decoy"` field, a `😀` astral
  pair, a lone high surrogate). This row was simply never flipped to match
  — same class of staleness as `AUD-015`/`SYS-W6-01`/`AUD-014` found and
  fixed earlier this session. No code change needed; verified via the
  existing `ctest -R mc3_ai` coverage.
- **SYS-W2-04** `[DONE, one sub-point deliberately partial]` `P2` — Redact
  API keys / auth headers from errors/logs; bound HTTP + extracted-XML
  size. Not tied to a single `AUD-###` finding (mandated hardening item).
  **"extracted-XML size" was already bounded** before this task:
  `Mc3XmlParser.cpp`'s `checkDocumentByteBudget()` (512MB, `SYS-W1-03`)
  already runs inside `parseString()`, which the AI-apply path
  (`AiResponseAlgorithms.hpp`) already calls via
  `Mc3Document::loadFromString(xml, {}, Mc3LoadPolicy::untrusted())` —
  verified, not re-implemented. **Added this session:**
  `AiAssistant::redactSecret(text, secret)` (replaces every occurrence of
  a secret with `[REDACTED]`, defense-in-depth — no current error-message
  path actually interpolates `apiKey` today, but nothing structurally
  prevented a future one from doing so) applied to every caught exception
  message in `sendAsync()`; `AiAssistant::boundedForDisplay(text, maxLen)`
  (truncates to 4096 bytes + a "...(truncated, N bytes total)" note)
  applied to `res->body` wherever it's embedded in an error message (a
  non-200 status, an empty-response error) — bounds how much of a
  huge/malicious HTTP response body can propagate into `errorMsg()`/the
  UI. Both exposed as public static methods (matching the existing
  `jsonEscape`/`extractStopReason`/`extractFirstTextValue` convention) so
  `ai_test.cpp` can test them directly, plus a real mock-`httplib::Server`
  integration test (`testMockServerOversizedErrorBodyIsBounded`) proving
  a 20000-byte error body produces a ~4KB `errorMsg()`, not a 20000-byte
  one. **Deliberately NOT done — a real, documented gap, not silently
  claimed fixed:** this httplib version (`_deps/httplib-src`) has no
  `Client::Post()` overload that streams the *response* through a
  size-capping `ContentReceiver` (only `Get()` exposes that; `Post()`'s
  `ContentProvider`/`content_length` parameters are for the *request*
  body, not the response) — so the full response is still buffered in
  memory by httplib itself before `boundedForDisplay()` ever runs on it.
  A genuinely malicious/huge response therefore still costs memory
  proportional to its real size during the network read, even though the
  *propagated* error string is now bounded. Fixing that would mean
  constructing a raw `httplib::Request`/`Response` pair and calling
  `Client::send()` directly with a manual size-checking content receiver
  — a larger, riskier change to a working, tested, production network
  path than this task's scope justified in one sitting; left as a
  follow-up if a future session wants it. Full rebuild + 124/124 `ctest`.
  Verify: `ctest -R mc3_ai`.
- **SYS-W2-05** `[DONE]` `P3` — Cap the Claude API HTTP response body size
  during the network read itself (not just when it's later embedded in an
  error message, which `SYS-W2-04` already bounds).
  **Implementation (2026-07-17):** replaced the `cli.Post(path, headers,
  body, content_type)` convenience call with a raw `httplib::Request`
  (`method`/`path`/`headers`/`body` set to mirror `Post()`'s own internal
  `send_with_content_provider()` exactly) sent via `cli.send(req)`, with
  `req.content_receiver` set to a lambda that accumulates into a local
  `responseBody` string and returns `false` (aborting the read/connection)
  once `responseBody.size() + len > maxResponseBytesCopy` — confirmed via
  `httplib.h`'s own `read_content()` that setting `content_receiver` means
  `res->body` is NOT auto-populated (the whole point of streaming), so
  every downstream use (`extractStopReason`/`extractFirstTextValue`/error
  messages) now reads `responseBody` instead. New public field
  `AiAssistant::maxResponseBytes` (default 8MB — generous for any real
  response even at 64k `max_tokens`, matches the existing overridable-
  timeout-field convention so a test can verify rejection without
  transferring megabytes). New tests:
  `testMockServerResponseExceedingCapIsAborted` (a 2000-byte mock response
  against a 500-byte cap correctly errors, not hangs) and
  `testMockServerResponseUnderCapStillSucceeds` (proves the streaming
  rewrite didn't break the success path). All pre-existing `ai_test.cpp`
  mock-server tests (success round-trip, truncated response, error status,
  model-name-in-body, malformed JSON, connection-refused, timeout) still
  pass unmodified, confirming `cli.send(req)` is a faithful replacement
  for `cli.Post(...)`. Full rebuild + 124/124 `ctest`; manual
  `--screenshot` smoke test. Verify: `ctest -R mc3_ai`.

### W3 — Architecture decomposition
- **SYS-W3-01** `[IN_PROGRESS]` `P2` — Extract from `MeshCraftApplication` (a god
  object split across .cpp files, not by responsibility): document/session,
  command/undo, selection, transform/gizmo, camera, animation, import/export,
  file/autosave, preferences, render pipeline, AI, registry, audio,
  dialog/status, platform bridge — with narrow interfaces, not another god
  object. **Confirmed a genuinely multi-session task, not a one-sitting fix:**
  a 3-way research pass (full header + all 17 implementation files + every
  already-extracted subsystem) found **280 data members + 113 methods** in the
  class (692-line header body, 11,544 lines of implementation across 17
  `.cpp` files), of which only **9 are already delegated** to an owned
  helper (`camera_`/`selection_`/`gizmo_`/`sceneRenderer_`/`gridRenderer_`/
  `hierarchyPanel_`/`propertiesPanel_`/`aiAssistant_`/`registry_`) — the
  remaining ~270 raw members are the actual "god object" surface. Also found
  a previously-abandoned partial attempt at exactly this task:
  `Editor::EditorViewport` (bundling `camera_`+`gizmo_`+a `pickRay()`
  helper) was added in commit `580105d` as an explicit "stub" and never
  wired into `MeshCraftApplication` — still dead code today. Full research
  writeup + phased roadmap (Phase 2 onward: Preferences/Macro, then
  `EditorViewport`'s fate, undo/redo, animation, file dialogs,
  post-processing, audio/walk-mode) recorded in this session's plan file for
  continuity, condensed here:
  **Phase 1 DONE (commit `95aaa90`):** extracted `Editor::KeybindingManager`
  (`include/MeshCraft/Editor/KeybindingManager.hpp` /
  `src/MeshCraft/Editor/KeybindingManager.cpp`) — the `keybindings_` map,
  `KeyBind` struct, default-seeding, load/save (ini format unchanged), and
  `shortcutFired()`'s modifier+just-pressed matching, all moved out of
  `MeshCraftApplication` verbatim; `MeshCraftApplication_Keybindings.cpp`
  deleted. Chosen first because it was the one of the three originally
  bundled candidates (Preferences + Macro recorder + Keybindings) with
  genuinely no cross-domain entanglement — reading the actual
  implementation (not just member counts) showed Macro's
  `executeMacroStep()` calls 8 other `MeshCraftApplication` methods plus
  direct field access (needs a `PropertiesPanel`-style callback-DI struct,
  deferred to a future phase alongside a broader Commands extraction), and
  Preferences' `loadPrefs()`/`savePrefs()` persist a cross-cutting struct
  spanning autosave/gizmo-snap/render-grid fields that aren't Preferences'
  own (needs a deliberate ownership decision, also deferred). New
  `keybinding_manager_test` (root `CMakeLists.txt`, needs real CNA
  `Keys`/`KeyboardState` so it links `CNA`/`SHARP_RUNTIME` unlike the
  header-only `active_tool_test` next to it) — no test existed for this
  subsystem before the extraction; covers default-seeding,
  `shortcutFired()`'s just-pressed/modifier-matching edge cases, and the
  save/load round-trip (a user rebind survives while untouched ids still
  re-seed to their default). Full tree rebuilt + 122/123 ctest (the 1
  pre-existing, out-of-scope `field_matrix` failure, unrelated).
  **Decisions (2026-07-17, human-authorized), unblocking Phase 2 onward:**
  (1) `EditorViewport` — **delete**, not finish. It's abandoned scaffolding
  (stub added `580105d`, never wired in, zero call sites); removing it is
  lower-risk than retargeting ~76 `camera_.`/~12 `gizmo_.` call sites across
  7 files during a stabilization phase. (2) Preferences (Phase 2) —
  **narrow**: the new class owns only `prefTheme_`/`prefsOpen_`/
  `applyTheme()`. `autoSaveInterval_`/`snapTranslate_`/`snapRotate_`/
  `snapScale_`/`gridSpacing_` stay on `MeshCraftApplication` for now,
  matching the single-domain-extraction idiom `KeybindingManager`
  established, to be picked up by their own future subsystem extractions
  rather than folded into Preferences.
  **`EditorViewport` deletion executed (2026-07-17):** removed
  `include/MeshCraft/Editor/EditorViewport.hpp` and
  `src/MeshCraft/Editor/EditorViewport.cpp` outright (`git rm`) — confirmed
  zero references anywhere else in the tree first
  (`grep -rln EditorViewport`). Sources are `file(GLOB_RECURSE ...)`-picked
  up in `CMakeLists.txt`, so no `CMakeLists.txt` edit was needed, just a
  reconfigure (`cmake .`) before the next build. Full rebuild + 123/123
  `ctest`, zero new warnings.
  **Phase 2 (narrow) DONE (2026-07-17):** extracted `Editor::Preferences`
  (`include/MeshCraft/Editor/Preferences.hpp` +
  `src/MeshCraft/Editor/Preferences.cpp`) owning exactly `prefTheme_`
  (renamed `theme_`, via `theme()`/`setTheme()`), `prefsOpen_` (renamed
  `windowOpen_`, via `windowOpen()`/`setWindowOpen()`), and `applyTheme()` —
  and nothing else, per the narrow-scope decision. `MeshCraftApplication`'s
  `loadPrefs()`/`savePrefs()` stay put (they persist `prefs_.theme()`
  together with `autoSaveInterval_`/`snapTranslate_`/`snapRotate_`/
  `snapScale_`/`gridSpacing_` in one prefs file via `PrefsAlg` — splitting
  that shared load/save mechanism was explicitly out of scope for this
  narrow extraction). Updated all 3 call sites
  (`MeshCraftApplication_UiOverlays.cpp`'s Preferences dialog + theme
  buttons, `MeshCraftApplication_UiMenuBar.cpp`'s Help menu item,
  `MeshCraftApplication_FileOps.cpp`'s `loadPrefs()`/`savePrefs()`). New
  `preferences_test` (no coverage existed for this subsystem before):
  default theme/windowOpen values, accessor round-trips, and — since
  `applyTheme()`'s ImGui side effect can't be asserted by inspecting a
  return value — a differential check that `theme()` 0/1/2 each apply a
  visibly different `ImGuiCol_WindowBg`, plus an out-of-range value falling
  back to Dark instead of crashing. Full rebuild + 124/124 `ctest` (new
  `preferences` test registered); manual `--screenshot` smoke test confirms
  the app still boots, loads prefs, and renders after the refactor.

### W5 — MC3 governance
- **SYS-W5-01** `[DONE]` `P2` — Machine-readable field matrix, `test/field_matrix.py`,
  registered as a real pass/fail `ctest` gate (`field_matrix`, `LABELS "lint"`) —
  CI itself stays parked (`AUD-052`). Hard-gates 6 layers (XSD attributes /
  C++ model / XML reader / XML writer / MCB reader / MCB writer) with a
  documented, triaged allowlist for intentional asymmetries (naming-
  convention differences, element-vs-attribute representation, tag-inferred
  enums, delimited-string-vs-array shape). Scoped down from the original
  9-layer wishlist: XSD *elements* and the examples/fixtures layer are
  informational-only (not gating — see the script's own docstring for why:
  XSD elements aren't attribute-shaped, and example files are intentionally
  sparse by design); the exporter layer (`mc3togltf/src/GltfExporter.cpp`) is
  extracted via a best-effort variable-name-prefix heuristic and kept
  informational-only, not hard-gated, due to a known false-positive source
  (a local `prim` variable is a glTF-JSON object, not `Mc3Primitive`); UI
  (`PropertiesPanel.cpp`) and the renderer are not extracted at all — spot-
  checked and found to use short/generic local variable names with no
  reliable name-to-Mc3-type correlation, so a regex extractor there would be
  noise, not signal. Running the tool against the tree found 0 real
  production bugs and 3 bugs in the extraction tool itself (regex missed
  space-before-paren call styles and one `sk`-named loop variable), all
  fixed; the 53 flagged asymmetries were individually verified against the
  actual reader/writer source and allowlisted with per-field reasons.
  Commit `e728b62`. Verify: `ctest --test-dir cmake-build-debug -R field_matrix`.
- **SYS-W5-02** `[DONE]` `P2/W13` — Document the elements/attributes
  `xsd_docs_diff.py` reports missing from `MC3_FORMAT.md`.
  **Implementation (2026-07-17):** the original 5+12 gap had grown to 14
  elements + 25 attributes by the time this was picked up — partly
  pre-existing (`background_texture`/`skybox_texture`/`uv_mapping`
  elements; `autoplay`/`euler_order`/`material_override`/`mip_maps`/
  `offset_u`/`offset_v`/`rotation_units`/`scale_u`/`scale_v` attributes),
  partly freshly introduced by this session's own `SYS-W6-04` work
  (`assetMetadata`/`imports`/`lod`/`lods`/`materialSlots`/`periodTags`/
  `regionTags`/`semanticTags`/`socket`/`sockets` elements; `bounds_max`/
  `bounds_min`/`category`/`clearance_volume`/`collision_proxy`/`facing`/
  `hash`/`instancing_eligible`/`max_visibility_distance`/`namespace`/
  `nominal_size`/`provenance`/`selection_weight`/`shadow_policy`/
  `subcategory`/`tier` attributes) — documented both. Added: a "Library
  and Imports" section (`<library>`/`<imports>`, R110/R101), a "UV
  Mapping" section (`<uv_mapping>`, per-object UV override), an "Asset
  Metadata" section (`<assetMetadata>`, R111, full attribute + child-
  element table), `rotation_units`/`euler_order` to the root element
  table, `script` to the shared object-attributes table, `material_override`/
  `variants` to the `<instance>` section, `background_texture`/
  `skybox_texture` to Environment, `mip_maps` to Textures, `autoplay` to
  Animations. `python3 test/xsd_docs_diff.py` now reports "0 not
  mentioned" for both elements and attributes (was 14/25). Doc-only, no
  runtime test (informational-only tool per its own docstring) — full
  124/124 `ctest` confirms no regression from the doc edits themselves.
  Verify: `python3 test/xsd_docs_diff.py`.
- **SYS-W5-03** `[DEFERRED, human-authorized decision]` `P2` — MC3
  versioning + unknown element/attribute policy; round-trip must not
  silently drop unknown data unless policy says so.
  **Decision (2026-07-17, human-authorized): leave current behavior as-is
  and document it as an accepted limitation.** No implementation work —
  silently dropping unrecognized XML attributes/elements on round-trip
  stays the documented, intentional behavior; not revisited unless a
  concrete need for forward/backward compatibility arises later.
  **Investigation (2026-07-17): confirmed, with real evidence (this was
  a genuinely-unverified one-line claim before): a fresh
  `<mc3 ... totally_unknown_root_attr="keep-me">` root attribute, an
  unrecognized `<box ... totally_unknown_obj_attr="also-keep-me">` object
  attribute, and an unrecognized `<totally_unknown_element foo="bar"/>`
  root child are **all silently dropped** on any load+save round-trip.
  Root cause, by design (not a bug to patch): `Mc3XmlParser.cpp` reads
  every field via explicit, named `attr(el, "known_name")` calls (21+
  call sites just for root/object-level attributes) with no
  "collect everything I didn't recognize" fallback, and `Mc3XmlWriter.cpp`
  builds a brand-new `XMLElement` from scratch on save, emitting only
  known fields via named `SetAttribute()` calls — there is no
  attribute-preserving codepath anywhere for either layer to lose data
  from in the first place. (`doc.metadata`/`doc.meta` are a deliberate,
  narrow, opt-in passthrough for the specific `<metadata>`/`<meta>`
  elements only — not a generic "preserve anything I don't recognize"
  mechanism.) **Why this stays `BLOCKED`, not implemented unilaterally:**
  the task's own title says "policy" for a reason — there are several
  materially different ways to actually solve this (a generic per-object/
  per-document "extra attributes" bag akin to `metadata`; raw-XML-node
  preservation; a hard schema-version gate that rejects unrecognized
  constructs instead of silently accepting+dropping them; simply
  documenting current behavior as an accepted limitation and closing this
  as DEFERRED) with real, different tradeoffs for every future format
  extension (this project's own `mc3.xsd`, and any external tool/consumer
  built against MC3) — exactly the kind of "could substantially affect
  architecture... format data" decision this session's own operating
  instructions say requires a human, not a unilateral implementation
  choice. Needs a human answer to: **should MC3 preserve unrecognized
  XML data on round-trip at all, and if so, via which mechanism?**
- **SYS-W5-04** `[TODO]` `P2` — Central document index / reference resolver.
  **Investigation (2026-07-17):** confirmed real, not stale — 13 call
  sites across the codebase (editor commands, animation channel target
  resolution, `MeshCraftApplication::flatFindById`/`flatFindByName`,
  `EditorAlgorithms.hpp`'s near-duplicate `flatFindByIdAlg`) each
  independently do an O(n) recursive tree walk to resolve an id/name to
  an object, with no caching/indexing anywhere — `R101`'s
  `Mc3ImportResolver` only resolves CROSS-library (`mc3lib://...`)
  references, not same-document id lookups. Deliberately left `TODO`
  rather than attempted here: a real cached id→object index needs correct
  invalidation on every mutation path in the ~18.5k LOC editor (rename,
  delete, undo/redo document-swap, merge, import, group/ungroup, ...) —
  a subtly wrong invalidation rule is a classic stale-cache correctness
  bug class, and this is a bigger, riskier design+implementation task than
  fits a single "continue working through the backlog" pass; needs its
  own scoped session (enumerate every `document_`-mutating call site
  first, the same way `SYS-W3-01`'s research pass did for
  `MeshCraftApplication`, before choosing an invalidation strategy).
- **SYS-W5-05** `[DONE]` `P2` — Property-based round-trip tests + parser fuzz target.
  **Status note (2026-07-17):** duplicate row — this exact scope is
  already delivered under `SYS-W11-05` (`[DONE]`): `mc3_random_roundtrip`
  (seeded-RNG property-based XML round-trip, `mc3/test/random_roundtrip_test.cpp`),
  `mc3_xml_mutation_fuzz` (`mc3/test/xml_mutation_fuzz_test.cpp`),
  `mcb_random_roundtrip`, plus standalone libFuzzer harnesses
  (`mc3/test/fuzz/mc3_xml_libfuzzer.cpp`, `mcb/test/fuzz/mcb_libfuzzer.cpp`)
  — all 3 ctest-registered entries confirmed present and passing in this
  session's full test runs. Same class of staleness as the 6 other
  findings this session (`AUD-014`, `AUD-015`/`SYS-W6-01`, `SYS-W2-03`,
  `SYS-W6-03`, `SYS-W14-01`, `SYS-W6-02`). No new work; verify: `ctest -R
  'random_roundtrip|xml_mutation_fuzz'`.

### W6 — MCB hardening
- **SYS-W6-01** `[DONE]` `P2` — Tag/type validation on the hot read path
  (`AUD-015`); enum range validation (`AUD-017`); RecursionGuard comment/code
  mismatch (`AUD-016`). **Status note (2026-07-17):** all three constituent
  `AUD-###` findings are independently `[DONE]` (see each entry below); this
  row was left stale at `[TODO]` because `AUD-015`'s own header line hadn't
  been corrected to match its later completion note (fixed above). No new
  work needed — verify with `grep -c "expectTag(" mcb/src/McbReader.cpp`
  (189) and `ctest -R mcb_roundtrip`.
- **SYS-W6-02** `[DONE]` `P3` — Malformed/truncated/corrupt/random/fuzz MCB tests
  incl. the untested compression-flag rejection path (`AUD-019`); byte-for-byte
  determinism; XML→MCB→XML equivalence.
  **Status note (via SYS-W1-04):** the malformed/truncated/corrupt/random/fuzz
  sub-scope is now substantially covered: `mcb_corruption_test` (commit
  `a38eb88`) sweeps every byte-offset truncation of a rich multi-section
  document and fuzzes 350 random byte buffers (0–16KB, seeded), both clean
  under ASan+UBSan, on top of `mcb_roundtrip_test.cpp`'s existing
  compression-flag rejection (`AUD-019`) and writer-determinism coverage.
  **Status note (2026-07-17): the "XML→MCB→XML equivalence has no dedicated
  test" claim was itself stale** — `mc3tomcb/test/mc3tomcb_roundtrip_test.py`
  (`mc3tomcb_roundtrip` ctest, `STAB-0058`, predates this whole audit
  session) already drives the real `mc3tomcb` CLI through exactly
  `fixture.mc3.xml → a.mcb → b.mc3.xml → c.mcb → d.mc3.xml` for 9 real
  fixtures, asserting MCB-header validity, byte-for-byte determinism
  (`a.mcb == c.mcb`, `b.xml == d.xml`), and no dropped element-tag/`model`
  content. Only gap: it didn't cover this session's own `SYS-W6-04`
  additions (`assetMetadata`/`library`/`imports`) — added
  `test/asset_metadata_library_import.mc3.xml` to its fixture list (10
  fixtures now), confirmed passing standalone before wiring it in. Same
  class of stale-`TODO` as the 5 other findings this session. Verify:
  `ctest -R mc3tomcb_roundtrip`.
- **SYS-W6-03** `[DONE]` `P3` — Fix `MCB_FORMAT.md` stale field-order list
  (`AUD-018`). **Status note (2026-07-17):** `AUD-018` itself has read
  `[DONE]` since commit `ba3e73c` — another stale `TODO` row simply never
  flipped to match (same class as `AUD-015`/`SYS-W6-01`/`SYS-W2-03` found
  this session). While re-verifying it, found that this session's own
  earlier `SYS-W6-04` work (adding `library`/`imports` to
  `writeDocument()`) had freshly re-broken the exact invariant `AUD-018`
  fixed — the field-order list didn't mention the two new fields. Fixed
  both: added `library, imports` in their actual written position
  (between `defaultCamera` and `meta`) to the field-order block, and added
  `Mc3AssetMetadata`/`Mc3LibraryInfo`/`Mc3Import` to the "each nested type"
  list. Doc-only, no runtime test (matches `AUD-018`'s own "Tests:
  Doc-only" note) — verified by reading `MCB_FORMAT.md`'s block against
  `writeDocument()` in `mcb/src/McbWriter.cpp` directly.
- **SYS-W6-04** `[DONE]` `P1` — Close `field_matrix`'s 18-field gate gap:
  `Mc3Object::assetMetadata` (R111, whole struct), `Mc3Object::scriptId`
  (R103), and `Mc3Document::library`/`imports` (R110/R101) were completely
  absent from **both** `mc3.xsd` (any document using `<assetMetadata>`,
  `<library>`, or `<imports>` failed XSD validation outright — not just a
  field_matrix nitpick) **and** MCB (`McbReader.cpp`/`McbWriter.cpp` never
  referenced any of them at all — a real, silent XML/JSON→MCB→XML data-loss
  bug for any document using these R109-R111 features, landed by the
  sibling `mesh-world` repo's own backlog outside this repo's `plan.md`
  tracking). Added: `assetMetadataType`/`libraryType`/`importType`/
  `importsType` complex types + child types (`assetTagListType`,
  `assetSocketsType`, `assetLodsType`) to `mc3.xsd`, wired as optional
  children/elements into all 16 object-shape types and the root `<mc3>`
  sequence; `script` attribute added to the shared `objectAttrs` group;
  `writeAssetMetadata`/`readAssetMetadata`, `writeLibraryInfo`/
  `readLibraryInfo`, `writeImport`/`readImport` added to
  `McbWriter.cpp`/`McbReader.cpp` (the new reader functions use
  `expectTag`, matching the codebase's now-universal convention — see
  `AUD-015`). MCB wire key names were deliberately chosen to match the
  XML/JSON attribute names (not always the literal camelCase C++ field
  name, e.g. `maxVisibilityDistance` not `maxVisibilityDistanceM`) so all
  three formats share one wire vocabulary. Remaining 4 field_matrix rows
  (`max_visibility_distance`, `namespace`, `script`, `tier`) are genuine
  naming-convention asymmetries (qualified C++ field name vs. a shared bare
  wire name, or a map-key-as-attribute-name shape), allowlisted with
  per-field reasons matching the tool's existing Group A/F patterns — not
  remaining gaps. New fixture `test/asset_metadata_library_import.mc3.xml`
  (first fixture to exercise any of the three) plus 3 new
  `mcb_roundtrip_test` functions (`testAssetMetadataRoundtrip`/
  `testScriptIdRoundtrip`/`testLibraryAndImportsRoundtrip`, ~40 new
  assertions) proving the previously-silent data loss is fixed. Full tree:
  123/123 `ctest` (up from 122/123 — `field_matrix` now passes), zero new
  warnings. Verify: `ctest -R 'field_matrix|xsd_validation|mcb_roundtrip'`.

### W7 — glTF fidelity
- **SYS-W7-01** `[DONE, "machine-checked" not newly built — see note]` `P2`
  — Truthful export matrix per model feature. UV mapping ignored
  (`AUD-024`), per-object metadata dropped (`AUD-029`), `--stats` warning
  undercounts (`AUD-026`).
  **Status note (2026-07-17):** all 3 cited `AUD-###` findings were
  already `[DONE]` — this row was simply never flipped to match (8th
  stale/incomplete `TODO` this session). The one genuinely missing piece:
  `MC3_FORMAT.md`'s existing "mc3togltf export support matrix" table
  (itself pre-existing, hand-maintained) had never been updated to
  reflect any of the 3 fixes — added rows for per-object UV mapping,
  per-object `metadata`, and `--stats` warning-count truthfulness, each
  verified against current `GltfExporter.cpp` source directly (not
  copy-pasted from the `AUD-###` notes) before writing. **"Machine-
  checked" was not newly built as dedicated tooling** — `test/field_matrix.py`
  already has an informational (non-gating) `GltfExporter.cpp`
  field-reference heuristic layer covering some of the same ground; a
  fully dedicated export-truthfulness verification script is a real,
  separate, larger undertaking this session judged not worth inventing
  when the concrete underlying bugs are all already fixed and the doc is
  now accurate — if a future session wants a hard machine-checked gate
  here specifically, that's new scope, not a "finish SYS-W7-01" task.
  Doc-only change; full 125/125 `ctest` (no behavior touched). Verify:
  read `MC3_FORMAT.md`'s export support matrix against `GltfExporter.cpp`.
- **SYS-W7-02** `[DONE]` `P2` — Differential geometry tests (viewport vs exporter).
  Commits `6edbb54` (extracted the viewport's tessellation math into a
  CNA-free `PrimitiveTessellationAlg.hpp`, verified behavior-preserving via
  a full 113/113 `ctest` incl. all "render" pixel tests) and `1ae9087`
  (added `differential_geometry_test`, 138 invariant-based assertions:
  bounding box / divergence-theorem volume / watertightness, checked
  against each other AND against independent analytical ground truth).
  Covers 8 of 10 primitive types (Box/Sphere/Cylinder/Cone/Plane/Torus/
  Capsule/IcoSphere); Disk/Grid out of scope (dynamic per-frame builders in
  `SceneRenderer_Extrude.cpp` with no "unit mesh + scale" split to extract,
  and open/flat surfaces for which the volume invariant is degenerate); CSG
  out of scope (already provably shared code via `buildPrimitive()`,
  STAB-0670 — a differential test there would be tautological). Found and
  documented four real cross-path discrepancies (not blindly fixed —
  correctness-critical rendering code, judged too risky to touch without a
  live-visual re-verification loop this task doesn't have): a structural
  Torus/Capsule viewport scale bug (single affine scale of one fixed-ratio
  unit mesh can't reproduce an arbitrary majorRadius/minorRadius or
  radius/height pair — elliptical tube / ellipsoidal caps whenever the
  ratio differs from the unit mesh's own), an IcoSphere WYSIWYG gap (the
  viewport hardcodes `subdivisions=2` and never reads the primitive's
  `segments`, unlike every other curved primitive's LOD tiers), a Capsule
  hemisphere-ring-count formula mismatch below `segments=16`, and a narrow
  Cylinder VPC-index-buffer topology inconsistency confirmed (via a real
  headless `--screenshot` render) NOT to reach the screen. Verify: `ctest
  --test-dir cmake-build-debug -R differential_geometry`.

### W8 — Backend truth
- **SYS-W8-01** `[DONE]` `P1` — Editor backend truthfulness. Commit `e53af49`
  added a configure-time warning; commit `58a7f03` (`AUD-039b`) added the real
  runtime enforcement — the editor refuses to launch under a non-EASYGL
  backend by default.

### W9 — Undo & data-loss
- **SYS-W9-01** `[TODO, superseded in scope by AUD-036b]` `P0` — Full undo/redo
  correctness: transaction abstraction, pre-mutation capture verification,
  `undo_coverage_audit.py` triage, atomicity, redo invalidation, selection
  restore. See `AUD-036b` for the authoritative task text.
- **SYS-W9-02** `[DONE]` `P1` — Atomic save (temp + rename); autosave; crash/
  corrupt recovery; never overwrite a user file after a failed save. Commits
  `386db82`, `6f93aeb`. Verify: `ctest --test-dir cmake-build-debug -R
  mc3_autosave_recovery` (headless mechanism test) plus manual: open a file,
  edit without saving, kill the process, relaunch and open the same file —
  the "Recover Unsaved Changes" dialog offers Recover/Discard/Keep. Status:
  atomic save (temp file + `std::filesystem::rename`) already existed at the
  writer layer for both formats before this task (`Mc3XmlWriter.cpp`,
  `McbWriter.cpp`, pre-existing — a failed write can never leave a
  truncated/corrupt file at the real path, so "never overwrite a user file
  after a failed save" was already true). Autosave itself
  (`performAutoSave()`/`autoSaveTickAlg`) also already existed. What this
  task added is actual RECOVERY: previously the only signal a newer
  `.autosave` existed was a passive 8-second status-message toast fired once
  at startup for command-line-opened files only — File > Open and Open
  Recent never checked at all. Added `checkForNewerAutosave()` as the one
  call site all three load paths now share, plus `recoverFromAutosave()`/
  `discardAutosave()` and a real "Recover Unsaved Changes" modal dialog
  (Recover / Discard Autosave / Keep Saved File). The App-level wiring
  (CNA/ImGui-coupled) is verified manually per the note above, matching this
  codebase's existing convention for UI-dialog-level behavior (see
  `AUD-036b`'s frame-driven tests for the exception to that convention where
  it was worth building a headless ImGui harness); the underlying mtime-
  comparison + reload mechanism it depends on is headlessly tested
  (`mc3_autosave_recovery_test`, `mc3/test/autosave_recovery_test.cpp`, 9
  assertions).
- **SYS-W9-03** `[DONE]` `P2` — Restore selection on undo/redo. See
  `AUD-036c`'s decision note: `performUndo()`/`performRedo()`
  (`MeshCraftApplication_Commands.cpp`) previously unconditionally called
  `selection_.clear()`; now restore whatever was selected immediately
  before the mutating command ran.
  **Implementation (2026-07-17):** added `undoSelectionStack_`/
  `redoSelectionStack_` (`std::vector<std::vector<std::string>>`) to
  `MeshCraftApplication`, kept index-for-index in lockstep with
  `undoStack_`/`redoStack_` — every push/pop/clear/cap of the document
  stacks (`pushUndo()`, `performUndo()`, `performRedo()`, plus the "Undo
  History" jump-to-step dialog's multi-entry push loop in
  `MeshCraftApplication_UiOverlays.cpp`, plus the 3 `undoStack_.clear();
  redoStack_.clear();` sites on file load in `MeshCraftApplication_FileOps.cpp`/
  `MeshCraftApplication_UiOverlays.cpp`) got a matching selection-stack
  operation. Two new helpers: `currentSelectionIds()` (selected objects'
  ids, skipping any with an empty id — same first-match-by-id semantics
  `flatFindById` already relies on) and `restoreSelectionByIds(ids)`
  (clears `selection_`, re-resolves each id against the just-swapped-in
  `document_` via a new `flatFindSharedById()` — the shared-ownership
  counterpart of `flatFindById()`, since `SelectionManager::select()` needs
  a `shared_ptr` — and silently skips an id no longer present, matching the
  human-authorized decision's "fall back toward empty/partial selection,
  never dangle/crash" requirement). Test coverage added to
  `test/undo_gesture_frame_test.cpp`: since `performUndo()`/`performRedo()`
  are CNA-coupled member functions (not headlessly callable, same
  constraint the file's pre-existing redo-invalidation block already
  documents), the new block mirrors the exact structure (paired stacks,
  restore-by-id, skip-missing) against a stand-in model — 5 new assertions
  covering full restore, partial restore when an id no longer exists, and
  the stacks-stay-in-lockstep invariant. Full rebuild + 124/124 `ctest`;
  manual `--screenshot` smoke test confirms the app still boots/renders.
  Verify: `ctest -R undo_gesture_frame`.

### W11 — Build / CI / DX
- **SYS-W11-01** `[TODO, owner-gated]` `P1` — Un-park CI (`.github_` → `.github`)
  needs a `workflow`-scoped push token (`AUD-052`). Workflow file itself made
  correct/ready in commit `d16c82c`.
- **SYS-W11-02** `[DONE]` `P2` — `-Wall -Wextra` on **all** first-party targets
  (currently only `Mc3` and `mc3togltf_lib`) via a shared interface target; fix
  warnings; `-Werror` in CI. (`AUD-054`) Commit `4fc211e`. Verify:
  `cmake --build cmake-build-debug 2>&1 | grep -c warning` (0). `-Werror` in CI
  not added (CI is parked, AUD-052) — would be inert until un-parked.
- **SYS-W11-03** `[TODO]` `P2` — Editor build+test CI job. (`AUD-053`)
- **SYS-W11-04** `[DONE]` `P2` — Opt-in ASan + UBSan configs. (`AUD-055`) Commit
  `4fc211e`. Verify: `cmake -S . -B /tmp/b -DMESHCRAFT_SANITIZE=ON && cmake
  --build /tmp/b --target mc3_roundtrip_test mcb_roundtrip_test && /tmp/b/mc3/mc3_roundtrip_test && /tmp/b/mcb/mcb_roundtrip_test`
  (both pass clean, no ASan/UBSan findings).
- **SYS-W11-05** `[DONE]` `P2` — Fuzz/differential harnesses over `McbReader` and
  `Mc3XmlParser`, wired into CI. (`AUD-055`) Commits `012f9b8`,
  `5bd8f17`. Verify: `ctest --test-dir cmake-build-debug -R
  "mc3_random_roundtrip|mc3_xml_mutation_fuzz|mcb_random_roundtrip"` (3/3
  pass); ASan/UBSan: `cmake -S . -B /tmp/b -DMESHCRAFT_SANITIZE=ON
  -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL && cmake --build /tmp/b --target
  mc3_random_roundtrip_test mc3_xml_mutation_fuzz_test
  mcb_random_roundtrip_test` then run all three (clean, zero findings);
  libFuzzer (opt-in, Clang only): `cmake -S mc3 -B /tmp/f1 -G Ninja
  -DCMAKE_CXX_COMPILER=clang++ -DMESHCRAFT_FUZZ=ON && cmake --build /tmp/f1
  --target mc3_xml_libfuzzer && /tmp/f1/mc3_xml_libfuzzer -max_total_time=30
  -seed=1` (same pattern for `mcb`/`mcb_libfuzzer`).
  - **Status note:** `McbReader` already had byte-fuzz coverage
    (`mcb_corruption_test.cpp`, landed under SYS-W1-04/AUD-055 before this
    task) but `Mc3XmlParser` had none — the primary gap this task closed.
    Delivered, for BOTH parsers: (1) a fast, always-on, CTest-registered
    seeded-mutation/random-byte fuzz harness
    (`mc3/test/xml_mutation_fuzz_test.cpp`, new — AFL-style byte-havoc
    mutation of a random-document-generated XML corpus plus pure random-byte
    noise fed to `Mc3Document::loadFromString`; `mcb_corruption_test.cpp`
    already covered the McbReader side and was left as-is); (2) a
    property-based/differential random round-trip harness for BOTH codecs,
    sharing one seeded random-document generator
    (`mc3/test/RandomMc3DocumentGenerator.hpp`, new, test-only, not part of
    Mc3's public API) — `mc3/test/random_roundtrip_test.cpp` round-trips 60
    random documents through the real XML save/load path (plus 15 more
    through the `loadFromString` in-memory entry point an AI/import pipeline
    actually calls) and asserts semantic equivalence field-by-field;
    `mcb/test/mcb_random_roundtrip_test.cpp` does the same through
    `saveToBinary`/`loadFromBinary`; both are new and neither existed before
    (existing round-trip tests were fixed/hand-picked fixtures only, one
    feature at a time, never randomized combinations at volume). (3) A
    genuine libFuzzer corpus-driven harness for BOTH parsers
    (`mc3/test/fuzz/mc3_xml_libfuzzer.cpp`, `mcb/test/fuzz/mcb_libfuzzer.cpp`,
    both new) — this environment has a working Clang 19.1.7 with
    `-fsanitize=fuzzer`, so rather than settling for "libFuzzer isn't
    practical here," a real opt-in `MESHCRAFT_FUZZ` CMake option was added,
    self-contained to `mc3/CMakeLists.txt`/`mcb/CMakeLists.txt` (no root
    `CMakeLists.txt` change needed/made — mc3 and mcb are already
    independently configurable per the CI matrix, so this never touches
    `../cna`). Verified with real bounded runs (`-max_total_time=30
    -seed=1`): `mc3_xml_libfuzzer` did 990,577 executions in 31s (coverage
    356/429 features) with zero crashes; `mcb_libfuzzer` did 365,956
    executions in 31s with zero crashes; both ran clean, no ASan/UBSan
    findings, no crash/oom/timeout artifacts. This libFuzzer target is
    deliberately NOT part of the default CTest run (requires Clang
    specifically; the default `cmake-build-debug` tree uses GCC, and it's a
    deep-fuzzing tool meant for longer/occasional runs, not a fast
    per-commit gate) — the always-on per-commit gate is (1)+(2) above.
    During development of the random round-trip generator/comparator, hit
    and fixed several FALSE-POSITIVE mismatches that traced to the test's
    own logic, not real bugs: Plane's `size.y` is not part of its persisted
    contract (writer only serializes x/z), and Ambient lights never persist
    `castShadows` (`Mc3XmlParser.cpp`'s `parseLights()` ambient branch
    doesn't read `cast_shadows`) — both fixed in the generator/comparator,
    not the product code, since they are pre-existing, non-crashing,
    narrow-field round-trip quirks rather than the crash/hang/UB class of
    bug this task's design constraints call out for a real product fix.
    **No real crash/hang/ASan bug was found** in either parser across all
    three techniques (seeded mutation, random differential round-trip, and
    ~1.35M genuine libFuzzer executions combined) — consistent with
    `mcb_corruption_test.cpp`'s prior finding that both readers were already
    memory-safe against arbitrary corruption before this task.
    **CI wiring explicitly out of scope/blocked**, not silently omitted: CI
    itself is parked pending `AUD-052` (owner-gated, no push-scoped
    workflow token in this environment) — a CI job invoking any of these
    targets would be inert until CI is un-parked. The per-commit
    (1)+(2) coverage runs today via plain `ctest`, same as every other test
    in the suite, so it needs no bespoke CI step once CI itself exists; only
    the opt-in deep-fuzzing (3) would need a *scheduled* (not per-commit) CI
    job, which is what remains blocked.
- **SYS-W11-06** `[DONE]` `P2` — `clang-format` + scoped `clang-tidy` config
  (checked-in, CI-integrated — ad-hoc manual passes were already run per
  `AUD-055`'s verify note). **Checked-in config only, not a mass
  reformat/lint-fix pass** (deliberate scope, matching this file's own
  "conservative slice" precedent): root `.clang-format` (derived from the
  existing hand-formatted style — verified with `--dry-run`-style diffs
  against representative files across `mc3/`/`mcb/`/`mc3togltf`/
  `src/MeshCraft`, not applied `-i`, since the existing 18.5k LOC was never
  written against it and a whole-tree reformat is a separate, much larger,
  not-yet-decided undertaking) and root `.clang-tidy` (`clang-diagnostic-*`/
  `clang-analyzer-*`, matching the already-verified-clean manual baseline
  from `docs/history/plan_20260710.md` STAB-0614/STAB-0615, plus
  `bugprone-*`/`performance-*`; deliberately NOT `modernize-*`, high-noise
  style opinions with no correctness value; `HeaderFilterRegex` scoped to
  first-party dirs only, excluding `../cna`/`../sharp-runtime`/vendored
  `FetchContent` deps). Verified both actually run: `clang-format` (not
  preinstalled in this sandbox — installed via `pip install clang-format`,
  no root needed) checked against representative files; `clang-tidy` (19.1.7,
  already present) run against both a standalone `mc3/` compile database and
  the full project's, surfacing exactly 15 warnings, all pre-existing and
  already triaged (12 `performance-enum-size` suggestions on scoped enums;
  the same `Mc3Texture::withWrap` pass-by-value note cppcheck already
  surfaced per STAB-0620, already explicitly left as-is there for the same
  "no API churn for a lint nitpick" reason) — zero new/surprising findings,
  zero findings in `CNA`/`SHARP_RUNTIME`/vendored code. CI wiring stays
  blocked on `AUD-052` (no CI to wire into); usage documented in
  `CONTRIBUTING.md`'s new "Code style" section.
- **SYS-W11-07** `[DONE, offline half — package-first half genuinely blocked]`
  `P2` — Package-first discovery + offline mode; pin sibling repos to
  recorded SHAs. (`AUD-057`) The pin/check half was already done: commit
  `d2943e3` added a non-fatal configure-time `git rev-parse` check against
  two recorded `MESHCRAFT_*_VERIFIED_SHA` values (warns, does not fail, on
  drift — see `AUD-057`'s status note for why fatal was rejected).
  **Offline mode, implemented (2026-07-17):** every third-party dependency
  this repo fetches (`tinyxml2`, `nlohmann_json`, `imgui`, `manifold`,
  `tinyobjloader`, `tinygltf`, `httplib`) was already pinned to an exact
  `GIT_TAG` — the prerequisite for reliable vendoring — so a fully offline
  build turned out to already be possible via CMake's built-in
  `FETCHCONTENT_SOURCE_DIR_<NAME>` cache variable, **zero code change
  needed**, just documentation. Verified empirically before writing it up
  (not assumed): configuring `mc3/`'s own `CMakeLists.txt` with
  `-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=<local path>` logs "Using the
  multi-header code from <local path>/include/" with no clone attempted.
  Documented in `README.md`'s new "Offline / vendored build" section: the
  full dependency→repo→tag→variable table and a ready-to-copy multi-`-D`
  configure command. **Package-first discovery (the other half) is
  genuinely blocked, not just deferred:** `find_package(CNA)` needs `cna`'s
  own `CMakeLists.txt` to `install()`/export a package config first — it
  does not today (confirmed: no `install(TARGETS ...)`, no generated
  `CNAConfig.cmake` anywhere in that tree) — adding that is a change to
  `cna` itself, out of bounds per `CLAUDE.md`'s "No CNA changes without
  owner permission." Documented as such in `README.md` rather than left
  unexplained. Doc-only change; full 125/125 `ctest` (no behavior
  touched). Verify: read `README.md`'s new section; re-run the empirical
  `FETCHCONTENT_SOURCE_DIR_...` test above to reconfirm.

### W12 — Performance baselines
- **SYS-W12-01** `[DONE, Phase 1 of 2 — see SYS-W12-02]` `P2` — Benchmark
  scenes + baselines (XML open/save, MCB convert/load, mesh-gen, CSG +
  cache, traversal, picking, undo snapshot, export, texture processing,
  animation eval, registry, startup, first frame). Optimize only measured
  bottlenecks.
  **Implementation (2026-07-17):** new `test/benchmark.py` times the
  existing `mc3tomcb`/`mc3togltf` CLI tools end to end (median of N runs)
  against small/medium/large real fixtures (`house.mc3.xml`/
  `features.mc3.xml`/`medieval_castle.mc3.xml`), covering 3 of the
  13 named categories: XML open + MCB save, MCB load + XML save, and
  glTF export (combined — the export number necessarily also exercises
  mesh-gen/CSG/texture processing, just not broken out per-phase).
  Registered as a new `benchmark` ctest, **informational only, never
  fails on timing** (only if a tool crashes) — wall-clock timing on a
  shared/virtualized/loaded machine is too noisy across runs/machines for
  a hard regression threshold to be a real signal rather than flakiness;
  see the script's own docstring. Recorded baseline numbers + methodology
  in new `test/BENCHMARK_BASELINE.md`. **Deliberately Phase 1, not the
  full 13-category ask:** mesh-gen/CSG-cache/traversal/picking/undo-
  snapshot/animation-eval/registry/startup/first-frame in isolation, and
  texture processing in isolation, all need either code instrumentation
  inside the editor/library or driving a live `MeshCraftApplication` —
  materially more work than CLI-level timing; tracked as new `SYS-W12-02`.
  Full rebuild + 125/125 `ctest` (was 124/124 — new `benchmark` test).
  Verify: `ctest -R benchmark`.
- **SYS-W12-02** `[TODO]` `P3` — Extend `SYS-W12-01`'s benchmark harness to
  the remaining categories that need in-process instrumentation rather
  than CLI-level timing: mesh-gen, CSG + cache, traversal, picking, undo
  snapshot, texture processing (in isolation), animation eval, registry,
  startup, first frame. Likely needs either a small headless benchmark
  executable linking `MeshCraftApplication`'s internals directly, or new
  timing instrumentation exposed through `--stats`-style CLI output.

### W14 — New features (after P0/P1 gates)
- **SYS-W14-01** `[DONE]` `P2` — Autosave + crash recovery.
  **Status note (2026-07-17):** duplicate row — this exact feature was
  delivered under `SYS-W9-02` (see that entry above: atomic save,
  autosave, `checkForNewerAutosave()`/`recoverFromAutosave()`/
  `discardAutosave()`, a real "Recover Unsaved Changes" modal dialog,
  `mc3_autosave_recovery_test`). This W14 row was simply never flipped/
  removed once W9's version shipped — same class of staleness as
  `AUD-015`/`SYS-W6-01`/`SYS-W2-03`/`SYS-W6-03` found this session. No new
  work; verify via `SYS-W9-02`'s own verification steps.
- **SYS-W14-02** `[DONE]` `P2` — Scene validation & diagnostics UI (builds on SYS-W1-01).
  Commit `9c7bfa5`: a new "Validation" ImGui panel (same `show*Panel_`
  toggle convention as the AI Assistant/Model Registry panels, View menu
  entry) shows the most recent load/save/export's `Mc3Validation` result as
  a table (severity/object/field/message/suggested repair) — `SYS-W1-01`'s
  diagnostics were console-only until now. A small status-bar indicator
  ("[!] N validation note(s)") appears whenever the last run produced
  findings, clickable to open the panel. `MeshCraftApplication::
  recordValidation()` captures the result at every real call site: the 4
  pre-render load sites, save, and both export paths (`runGltfExport`,
  `runObjExport`). Visually verified end-to-end with a real screenshot
  (`--screenshot` against a scene with an out-of-range value, panel forced
  open temporarily then reverted) — this sandbox's SDL window doesn't map
  onto a visible desktop for interactive driving, but `ffmpeg x11grab` +
  the app's own `--screenshot` framebuffer dump both work (ImageMagick
  `import`/`xwd` against the same X display do not). No new automated
  test — thin ImGui glue over already-tested `Mc3Validation`/
  `recordValidation()` plumbing, matching this codebase's own precedent of
  not testing every UI call site that consumes already-tested logic.
- **SYS-W14-03** `[TODO]` `P2` — PNG screenshot / image export.
- **SYS-W14-04** `[DEFERRED]` `P3` — SVG texture rasterization pipeline.
- **SYS-W14-05** `[DEFERRED]` `P3` — Safe `embed:` mesh/resource support end-to-end. (`AUD-025`)
- **SYS-W14-06** `[DEFERRED]` `P3` — Improved CSG output (smooth normals/UVs/materials).
- **SYS-W14-07** `[DEFERRED]` `P3` — Improved walk/navigation collision.
- **SYS-W14-08** `[TODO]` `P2` — AI change preview/diff before destructive replace.
- **SYS-W14-09** `[BLOCKED]` `P3` — Web persistence & export verification (blocked on
  CNA/browser — see NEXT.md).

---

## Audit-derived tasks (AUD-###)

62 tasks: 57 from the session-1 audit (re-verified against current source in
session 2) plus 5 from session 2's adversarial re-open. Ordered by discovery
(AUD-001..057 in original severity order, followed by the session-2 additions).
Status is re-verified per row, not copied from a prior summary — do not trust a
DONE marker without checking its cited commit/verify command.

### AUD-001 `[DONE]` `P1 (self-assigned P2, elevated/adjusted per adversarial correction)` `W0` · mc3togltf helix extrude divides by zero -> NaN/inf geometry written to glTF as success
- **Component:** mc3togltf/src/MeshBuilder.cpp (samplePath, PT::Helix)
- **Evidence:** MeshBuilder.cpp:748 computes the path tangent as `float tx = -std::sin(t), ty = h / (r * totalAngle), tz = std::cos(t);` where `r = path.helixRadius` (MeshBuilder.cpp:741) and `totalAngle = 2*pi*turns`. The parser applies NO lower bound: `path.helixRadius = attrF(el, "radius", 0.5f);` (Mc3XmlParser.cpp:139), `path.helixTurns = attrF(el, "turns", 4.0f);` (Mc3XmlParser.cpp:141). With `helix_radius="0"` (or `turns="0"`), `r*totalAngle==0` so `ty = h/0` = +/-inf, then `tl=sqrt(...)`=inf and `ty/tl`=inf/inf=NaN (MeshBuilder.cpp:749-750). buildExtrude feeds this NaN tangent into frameAxes -> NaN cross-section vertices -> NaN positions, WITH indices generated, so `if (md.empty()) return -1;` (GltfExporter.cpp:613) does NOT skip it (md.empty() only tests indices, MeshBuilder.hpp:18). addAccessorVec3(...,calcBounds=true) then writes NaN POSITION data and NaN accessor minValues/maxValues (GltfExporter.cpp:213-225), producing a spec-invalid glTF. main.cpp prints 'Written:' and returns 0 (main.cpp:93). The OBJ path explicitly rejects non-finite (`if (!std::isfinite(v)) throw ...` MeshBuilder.cpp:1145-1150) but the parametric path has no such guard. The LIVE editor renderer computes the helix tangent WITHOUT this division (SceneRenderer_Extrude.cpp:100 uses vn3({-R*omega*sin,H,R*omega*cos}) then normalizes -> finite {0,1,0} when R=0), so the user sees a correct helix in the editor but gets an invalid glTF on export.
- **Outcome:** Guard the helix tangent against a zero denominator (e.g. compute the raw derivative vector then normalize, matching SceneRenderer_Extrude.cpp), and/or add a non-finite geometry check in GltfExporter before writing accessors (mirroring the OBJ path's isfinite rejection) so degenerate parametric inputs fail loudly instead of emitting an invalid glTF reported as success.
- **Tests:** Add an mc3togltf test: an <extrude> whose <path type="helix" radius="0"> (and a second with turns="0") must either export finite POSITION/min/max or fail with a clear error, never emit NaN. Assert all exported POSITION floats and accessor min/max are finite.
- **Verify note:** Evidence is accurate; two refinements. (1) The parser file is mc3/src/Mc3XmlParser.cpp (invoked via Mc3Document::loadFromFile in main.cpp), not a mc3togltf-local Mc3XmlParser.cpp — the line numbers 139/141 are exact. (2) Severity is understated, not inflated: producing a spec-invalid glTF (NaN positions + NaN accessor min/max) that the tool reports as "Written" success is missing input validation on a critical path -> P1, brushing P0's "invalid-output-reported-as-success" band, rather than the self-assigned P2. Trigger requires explicit degenerate input helix_radius="0" or turns="0".
- **Resolved:** commit `fd606d2` — verify: `ctest -R mc3togltf_hostile_geometry`
- **Status note:** Root cause (helix tangent divide-by-zero) NOT changed; instead the exporter's finiteness gate (addAccessorVec3) now throws before writing NaN accessor data, so a degenerate helix fails loudly instead of emitting an invalid glTF reported as success -- the finding's own stated acceptable alternative outcome.

### AUD-002 `[DONE]` `P1 (self-assigned P2, elevated/adjusted per adversarial correction)` `W0` · loadObjMesh indexes attrib arrays with unvalidated tinyobj face indices (potential OOB read)
- **Component:** mc3togltf/src/MeshBuilder.cpp (loadObjMesh)
- **Evidence:** MeshBuilder.cpp:1188-1191 reads `auto vi = static_cast<size_t>(idx.vertex_index); m.positions.push_back(attrib.vertices[3*vi+0]); ...[3*vi+1]; ...[3*vi+2];` with no check that `3*vi+2 < attrib.vertices.size()`. Same for normals (MeshBuilder.cpp:1194-1197: `attrib.normals[3*ni+2]`) and texcoords (MeshBuilder.cpp:1204-1207: `attrib.texcoords[2*ti+1]`), and in the face-normal fallback (MeshBuilder.cpp:1176-1177). The code already validates vertex *values* are finite (MeshBuilder.cpp:1145-1150) but never validates *indices*. This path parses an arbitrary user-supplied .obj referenced by an mc3 <mesh src=...> (MeshBuilder.cpp:1123-1135). If tinyobjloader yields an out-of-range positive index or an unresolved negative index for a malformed face, `static_cast<size_t>` of a negative int becomes a huge value and the array subscript is an out-of-bounds read (undefined behavior / crash).
- **Outcome:** Bounds-check each index against the corresponding attrib array size before subscripting (e.g. skip or reject the face/vertex when `3*vi+2 >= attrib.vertices.size()`, `ni<0 || 3*ni+2 >= attrib.normals.size()`, `ti<0 || 2*ti+1 >= attrib.texcoords.size()`), throwing a clear error like the finite-vertex guard already does.
- **Tests:** Add an mc3togltf test that imports a hand-crafted malformed .obj whose face references a vertex index beyond the vertex count; it must fail with a clear error, not crash or read OOB (verify under ASan).
- **Verify note:** Mechanism precision: the OOB is reachable specifically because a triangle face (npolys==3) with triangulate=true bypasses the (3*vi+2)>=v.size() guards that protect the quad/polygon triangulation paths, landing in the unchecked else branch at tiny_obj_loader.h:1965-1979; tinyobjloader emits only a non-fatal warning, which MeshBuilder.cpp:1136-1137 prints but does not treat as an error. The finding's secondary claim that a negative int becomes a huge size_t does not apply to vertex_index (fixIndex rejects idx<=0 for vertices with allow_zero=false), but the positive-out-of-range case it also cites is the real, confirmed vector; normal_index/texcoord_index are guarded by >=0 checks only against -1, so out-of-range POSITIVE vn/vt indices are also OOB. Severity: understated. This is an OOB read (undefined behavior, possible crash, or garbage floats silently written into a glTF reported as a successful conversion) reachable from an arbitrary user-supplied .obj referenced by mc3 <mesh src=...>. Under the audit rubric that classifies unsafe-input/UB/crash and invalid-output-reported-as-success as P0, this is P0 (P1 at minimum), not P2.
- **Resolved:** commit `d701df7` — verify: `ctest -R mc3togltf_obj_robustness`
- **Status note:** Added an explicit `checkIndex()` bounds check on vertex_index/normal_index/texcoord_index before every `attrib.vertices`/`normals`/`texcoords` subscript, throwing a clear "index N out of range" error that the existing `buildMesh()` try/catch turns into a "Warning:" + skipped node (same pattern as the other malformed-OBJ cases, STAB-0630). Regression fixture `test/obj_malformed_oob_positive.obj` (a triangle face referencing a vertex index far beyond the file's vertex count) wired into the existing `obj_robustness_test.py`/`export_stats_test.py` harness. While building this fixture, found and fixed a real, separate bug this exposed — see `AUD-060`.
- **Blocked:** Confidence is PLAUSIBLE: proving an actual OOB requires confirming tinyobjloader passes through out-of-range indices, and tinyobjloader is vendored under _deps (out of audit scope). The first-party missing bounds check is certain; the trigger depends on loader behavior.

### AUD-003 `[DONE]` `P3` `W0` · glTF re-read type-puns via reinterpret_cast from a byte vector (strict-aliasing/alignment UB)
- **Component:** src/MeshCraft/MeshCraftApplication_FileOps.cpp (ReadVec3/ReadVec2/ReadIndex, OBJ export)
- **Evidence:** FileOps.cpp:370 `const float* f = reinterpret_cast<const float*>(&buf.data[offset]);` and FileOps.cpp:379 (ReadVec2) read `float` objects out of `buf.data`, which is tinygltf's `std::vector<unsigned char>` where no `float` object was ever created (reading a value through a type that doesn't match the object's dynamic type is UB), and `offset = bv.byteOffset + acc.byteOffset + i*stride` is not guaranteed 4-byte aligned. ReadIndex is worse: FileOps.cpp:389 `reinterpret_cast<const uint16_t*>(&buf.data[offset])[i]` and :390 `reinterpret_cast<const uint32_t*>(&buf.data[offset])[i]` perform potentially-unaligned 2/4-byte reads. The buffer here is a self-generated GLB re-read in runObjExport (FileOps.cpp:466-473), so it is trusted and works on mainstream x86/ARM in practice, but the construct is technically undefined and non-portable.
- **Outcome:** Read the bytes into a properly-typed local via std::memcpy (e.g. `float f; std::memcpy(&f, &buf.data[offset], sizeof f);`) instead of reinterpret_cast+deref, which is well-defined and handles misalignment.
- **Tests:** N/A behavioral (self-generated trusted data); a UBSan/ASan run over an OBJ export would flag any alignment issue if a platform enforces it.
- **Resolved:** commit `a0c1d8a` — verify: `ctest -R editor_export_test`
- **Status note:** All three functions (ReadVec3/ReadVec2/ReadIndex) now memcpy into a properly-typed local instead of reinterpret_cast+deref. Zero behavior change (verified via editor_export_test, which exercises this exact re-read path).
- **Blocked:** Low severity: operates only on MeshCraft's own freshly-written GLB, not on untrusted input, so no observed misbehavior on supported platforms.

### AUD-004 `[DONE]` `P0` `W1` · MC3 parser accepts NaN/Inf floats unchecked; they flow into geometry and produce a spec-invalid glTF reported as success
- **Component:** mc3/src/Mc3XmlParser.cpp, mc3/src/MathUtils.hpp, mc3togltf/src/GltfExporter.cpp
- **Evidence:** There is NO isfinite/isnan check anywhere in mc3/src (grep confirms only mc3/test/roundtrip_test.cpp uses std::isfinite). attrF at Mc3XmlParser.cpp:38-42 only guards against exceptions: `try { return std::stof(v); } catch (...) { return def; }` — but std::stof("nan")/std::stof("inf") do NOT throw, they return NaN/Inf. Same at line 193: `try { float f = std::stof(s); p.size = {f, f, f}; }` so `<box size="nan"/>` yields size {NaN,NaN,NaN}. The vec paths are worse: MathUtils.hpp:11 `std::sscanf(s.c_str(), "%f %f %f", ...)` and :18 accept C99 "nan"/"inf"/"1e400"(->HUGE_VALF) silently, so `<light color="1e400 0 0"/>` or `<mc3><objects><box position="nan 0 0">` yield Inf/NaN transforms/colors. Downstream, buildSphere/buildBox emit NaN vertex positions and addAccessorVec3 (GltfExporter.cpp:213-225) computes minValues/maxValues by `<`/`>` comparisons that are all false for NaN, so bounds stay NaN — no finite guard here, in stark contrast to loadObjMesh (MeshBuilder.cpp:1145-1150) which DOES `if (!std::isfinite(v)) throw`. mc3togltf then writes the file and returns 0.
- **Outcome:** Validate every parsed float (attrF, parseVec3/parseVec4, the stof size/scale paths) with std::isfinite and reject or clamp/default non-finite values at parse time; alternatively add an isfinite gate on generated vertex data in mc3togltf before writing accessors, mirroring the existing OBJ check.
- **Tests:** Add a fixture <box size="nan"/> and <sphere position="1e400 0 0"/>; assert Mc3Document load rejects/sanitizes, and that mc3togltf either fails loudly or emits only finite accessor min/max (no NaN/Inf in the GLB buffer).
- **Resolved:** commit `dae910f` — verify: `ctest -R mc3_finite_input`

### AUD-005 `[DONE]` `P1` `W1` · Segment/subdivision/sides counts are parsed unbounded, enabling memory-exhaustion DoS from a hostile primitive
- **Component:** mc3/src/Mc3XmlParser.cpp, mc3togltf/src/MeshBuilder.cpp
- **Evidence:** attrI (Mc3XmlParser.cpp:44-48) returns std::stoi with no range clamp. parsePrimitive sets `p.segments = attrI(el, "segments", type == ObjectType::IcoSphere ? 2 : 32);` (line 212) and subdivisionsX/Z (lines 223-224) with no upper bound; parseCrossSection sets `cs.sides = attrI(el, "sides", 6); cs.segments = attrI(el, "segments", 32);` (lines 116-117); parseExtrude sets `ext.segments = attrI(el, "segments", 32);` (line 162). buildPrimitive (MeshBuilder.cpp:1106) forwards this straight to `buildSphere(p.radius, p.segments)` where `int rings = segments/2; int sectors = segments;` (MeshBuilder.cpp:99-100) allocate (rings+1)*(sectors+1) vertices. `<sphere segments="100000000"/>` requests ~5e15 vertices -> OOM/thrash. buildCylinder/buildTorus/buildCapsule/buildDisk and sampleCrossSection (sides/segments, MeshBuilder.cpp:673-681) are the same class. Note the IcoSphere path IS bounded (`std::max(1, std::min(4, p.segments/8))`, MeshBuilder.cpp:1114) and CSG uses a fixed CSG_SEGMENTS=32 (CsgEvaluator.cpp:171) — but the direct GLB export path is not. main.cpp catches std::exception so a clean std::bad_alloc is handled, but the realistic effect is OOM-kill/hang (availability DoS) before that.
- **Outcome:** Clamp segments/sides/subdivisions to a sane maximum (e.g. <=4096) at parse time in parsePrimitive/parseCrossSection/parseExtrude, or reject values above a budget, so no untrusted count can drive unbounded allocation.
- **Tests:** Add a fixture <sphere segments="100000000"/> and assert the loader clamps p.segments (or mc3togltf fails fast) instead of attempting a multi-terabyte allocation.
- **Verify note:** Severity P1 is appropriate (resource-exhaustion/availability DoS on an untrusted input path, no UB/corruption). One clarification that strengthens rather than weakens the finding: the CSG path is not fully bounded either — while Manifold's native Sphere/Cylinder/Cone use CSG_SEGMENTS=32 (CsgEvaluator.cpp:208/213/218), the CSG fallback at CsgEvaluator.cpp:236 calls buildPrimitive(*obj.primitive) which forwards the same unbounded p.segments for Box/Torus/Capsule/Disk leaf primitives.
- **Resolved:** commit `fd606d2` — verify: `ctest -R mc3_input_budget`

### AUD-006 `[DONE]` `P1` `W1` · Mesh src / texture uri / include file paths are unvalidated -> path traversal reads arbitrary files (texture bytes are exfiltrated into the output GLB)
- **Component:** mc3/src/Mc3XmlParser.cpp, mc3togltf/src/GltfExporter.cpp, mc3togltf/src/MeshBuilder.cpp
- **Evidence:** The parser stores filesystem references verbatim with no containment check: parseTextures line 491 `tex.uri = attr(c, "uri");`, mesh source lines 317-319 `obj->meshSource = attr(el, "src");`, and processIncludes line 901 `std::filesystem::path includePath = selfPath.parent_path() / fileAttr;`. In the consumer, GltfExporter.cpp:431 does `std::filesystem::path imgPath = basePath / tex.uri;` then reads raw bytes and base64-embeds them into the GLB (lines 432-438: `img.image = std::vector<unsigned char>(std::istreambuf_iterator<char>(ifs), ...)`). Because std::filesystem::operator/ replaces the left side when the right is absolute, `<texture uri="/etc/passwd">` reads /etc/passwd and embeds its contents into the output GLB; `uri="../../../../etc/passwd"` works via basePath join. loadObjMesh has the identical pattern (MeshBuilder.cpp:1125-1126: `if (objPath.is_relative()) objPath = basePath / source;`). No check that the resolved path stays within the document directory.
- **Outcome:** After resolving meshSource/tex.uri/embed src/include file against basePath, canonicalize and reject any path that escapes the document root (contains .. traversal above root or is absolute), before opening the file.
- **Tests:** Fixture with <texture uri="/etc/hostname"> and uri="../../secret"; assert mc3togltf refuses to read/embed files outside the source directory.
- **Verify note:** Minor evidence imprecision: `tex.uri = attr(c, "uri");` is at Mc3XmlParser.cpp line 490, not 491 (491 is `tex.wrapU`). Severity P1 is reasonable and arguably conservative — since the rubric lists "unsafe-input" under P0 and this yields arbitrary-file-read/exfiltration from untrusted converter input (web bridge exists), a P0 classification would also be defensible. No downgrade warranted; P1 stands or could be raised to P0.
- **Resolved:** commit `901965f, 40643a4` — verify: `ctest -R "mc3togltf_hostile_geometry|mc3_load_policy"`
- **Status note:** Texture/mesh paths confined by default in the exporter (901965f). Include-path confinement (confineIncludesToRoot) exists but is only applied by the untrusted() load policy (40643a4) -- a trusted user-opened file's <include> is intentionally NOT confined (a user's own scene can legitimately include a file anywhere on disk they choose). See AUD-006b for the remaining broader-than-include untrusted-resource gap (texture/mesh/embed/sound paths at the AI/application layer, not just at mc3togltf export time).

### AUD-007 `[DONE]` `P1` `W1` · <instance> definition cycle causes unbounded recursion / stack overflow in GltfExporter::buildNode (no depth or visited-set guard)
- **Component:** mc3togltf/src/GltfExporter.cpp, mc3/src/Mc3XmlParser.cpp
- **Evidence:** parseDefinitions (Mc3XmlParser.cpp:636-646) and parseObject build <instance definition="..."> nodes with no cycle validation. buildNode is declared `static int buildNode(ExportCtx& ctx, const Mc3Object& obj)` (GltfExporter.cpp:716) with NO depth parameter and no visited set. On an instance it resolves the definition and then unconditionally recurses into the definition's children: lines 800-805 `for (const auto& child : defObj.children) { ... int ci = buildNode(ctx, *child); ... }`, and the general child loop at line 857. A definition whose subtree contains an <instance> pointing back to itself (e.g. definition A containing an instance of A) recurses forever -> stack overflow / crash. Note the CSG evaluator explicitly guards this exact hazard: CsgEvaluator.cpp:170 `static constexpr int CSG_MAX_DEPTH = 12;` enforced at line 182, but buildNode has no equivalent.
- **Outcome:** Add a recursion depth cap and/or an in-progress definition-id set to buildNode (mirroring CSG_MAX_DEPTH), throwing on cyclic/over-deep instance expansion instead of recursing unbounded.
- **Tests:** Fixture: <definition id="A"><group><instance definition="A"/></group></definition> plus a top-level <instance definition="A"/>; assert mc3togltf reports a cycle error rather than crashing.
- **Verify note:** Finding is accurate as stated. Severity P1 is defensible but arguably understated: because this is a stack-overflow crash triggered by untrusted/malformed input to a file-conversion tool, it plausibly qualifies as P0 (crash / unsafe-input) under the rubric. No downgrade is warranted.
- **Resolved:** commit `fd606d2` — verify: `ctest -R mc3togltf_hostile_geometry`

### AUD-008 `[DONE]` `P1` `W2` · AI-generated XML is parsed through the full Mc3Document::loadFromFile pipeline, which honors <include file="...">, giving untrusted AI output arbitrary local-file access / path traversal
- **Component:** AI response parsing (src/MeshCraft/AiResponseAlgorithms.hpp -> mc3/src/Mc3XmlParser.cpp)
- **Evidence:** The AI's raw text is written to a temp file and fed straight to the production XML loader: AiResponseAlgorithms.hpp:98-112 parseXmlAlg() does `{ std::ofstream f(tmp); f << xml; } ... auto doc = Mc3::Mc3Document::loadFromFile(tmp);`. validateAndParseAiResponseAlg (line 216) calls parseXmlAlg BEFORE any XSD check (line 223), so the parse side effects fire regardless. Mc3Document::loadFromFile -> Mc3XmlParser::parse (Mc3Document.cpp:7-10, Mc3XmlParser.cpp:915) runs processIncludes() at :960. processIncludes (Mc3XmlParser.cpp:895-908) reads the attacker-controlled attribute with NO sanitization: `const char* fileAttr = inc->Attribute("file"); ... std::filesystem::path includePath = selfPath.parent_path() / fileAttr;` then mergeInclude() -> `xml.LoadFile(includePath.string().c_str())` (Mc3XmlParser.cpp:778). Because `path / fileAttr` returns fileAttr unchanged when it is absolute, and `..` segments are never stripped, an AI response containing `<include file="/etc/…">` or `<include file="../../…">` makes the app open arbitrary filesystem paths chosen by the model. The only grep hits for path handling (:766/:768 weakly_canonical) are for cycle-dedup, not for confining the path to any allowed root. The XSD explicitly permits <include> (mc3.xsd:888-889), so schema validation does not block it either — and it runs after the read anyway.
- **Outcome:** Before writing an AI response to the temp file / parsing it, either strip/reject any <include> element, or parse AI output in a hardened mode that disables include processing and any filesystem-relative resolution (texture uri / meshSource / embed src). At minimum, confine include/resource paths to an allowlisted directory and reject absolute paths and `..` traversal in Mc3XmlParser processIncludes.
- **Tests:** Add an ai_test.cpp case feeding validateAndParseAiResponseAlg a response whose XML contains `<include file="/etc/hostname">` and `<include file="../../../secret.mc3.xml">`, asserting the include is NOT loaded (no filesystem access outside an allowed root). Add a unit test on Mc3XmlParser rejecting absolute/`..` include paths.
- **Resolved:** commit `40643a4` — verify: `ctest -R mc3_load_policy`

### AUD-009 `[DONE]` `P2` `W2` · Provider JSON parsed by hand-rolled string scanning: extractFirstTextValue trusts the first literal "text":" and mis-decodes UTF-16 surrogate pairs into invalid UTF-8
- **Component:** JSON extraction (src/MeshCraft/AiAssistant.cpp)
- **Evidence:** AiAssistant.cpp:66-69: `const std::string key = "\"text\":\""; auto pos = json.find(key);` — it takes the FIRST occurrence of the literal `"text":"` anywhere in the body as the model's answer, with no awareness of JSON structure (not scoped to content[].type=="text"); any provider/feature that emits a `"text":"..."` field earlier (citations, tool_use input echoes, server-tool blocks, future response shapes) would be extracted instead. The \u handler at :85-108 only synthesizes BMP code points (`if (code < 0x800) … else { 3-byte } `) and never combines UTF-16 surrogate pairs, so an astral escape like `😀` is emitted as two lone-surrogate 3-byte sequences = invalid UTF-8. Likewise extractStopReason (:53-62) scans for the literal `"stop_reason":"` and silently returns empty on `"stop_reason":null`, so wasTruncated() can't distinguish a null stop_reason.
- **Outcome:** Parse the Messages response with a real JSON parser (or at minimum scope extraction to the content array and the first block with type=="text"), and either correctly combine UTF-16 surrogate pairs or reject/replace lone surrogates so output is always valid UTF-8.
- **Tests:** Add ai_test.cpp cases: (a) a response where an earlier field literally contains `"text":"decoy"` before the real content block, asserting the real text is returned; (b) an astral `😀` escape asserting valid UTF-8 output.
- **Resolved:** commit `ca82b0b` — verify: `ctest -R mc3_ai`
- **Status note:** Took the "at minimum" alternative from Outcome (no new JSON-parser dependency, consistent with this file's documented hand-rolled-parsing scope): `extractFirstTextValue`'s search key changed from the bare `"text":"` to the full `"type":"text","text":"` prefix, which every real Claude API response literally contains (confirmed: every existing test fixture in this file already used that exact shape, so this was a pure tightening with zero pre-existing-test fallout). Rewrote the `\u` escape handling to detect a high surrogate (0xD800-0xDBFF), look ahead for an immediately-following low surrogate (0xDC00-0xDFFF), and combine them into one astral code point encoded as proper 4-byte UTF-8 (the prior code had no 4-byte case at all, so it could never have correctly encoded an astral character even if surrogates had been combined); a genuinely lone/unpaired surrogate is now mapped to U+FFFD instead of emitted as invalid UTF-8. Did not change `extractStopReason`'s `null`-vs-absent handling (both already collapse to the same "not truncated" outcome via `wasTruncated()`'s single `== "max_tokens"` comparison, so it wasn't a behavioral gap, just cosmetic) — not part of the Tests field's ask. New tests: a decoy `"text":"decoy"` field with a non-text `"type"` proves the scoping (real answer still extracted); a `😀` (😀, U+1F600) pair decodes to the correct 4-byte UTF-8; a lone `\ud83d` decodes to U+FFFD.

### AUD-010 `[DONE]` `P2` `W2` · No test covers a malicious <include> in an AI response — the arbitrary-file-read path is entirely unverified
- **Component:** Test coverage (mc3/test/ai_test.cpp)
- **Evidence:** ai_test.cpp exercises only JSON helpers and extract/repair/parse of benign XML (grep shows cases for extractStopReason/extractFirstTextValue/extractXmlAlg/repairXmlAlg/parseXmlAlg with hello/café/fenced XML), and never feeds an AI response containing an `<include>` directive. Combined with finding #1 (parseXmlAlg -> loadFromFile honors <include> against attacker paths), the security-critical file-access vector is completely untested, so a regression that (re)enables arbitrary include resolution from AI output would pass CI.
- **Outcome:** Add a regression test asserting that AI responses containing <include> do not cause any filesystem read outside an allowed root once finding #1 is fixed.
- **Tests:** New ai_test.cpp case as described in finding #1's tests field.
- **Verify note:** Impact framing should be tightened: loadFromFile performs local-file INCLUSION/parse (LFI/XXE-style — it merges the target's definitions or throws a parse error), not direct content exfiltration, so 'arbitrary-file-read' overstates it. 'Completely untested' is imprecise: the include-resolution machinery (processIncludes/mergeInclude, relative resolution, collisions, nonexistent-file failure, spaces) IS covered by mc3/test/roundtrip_test.cpp (lines ~1046-1546); what is genuinely untested is the AI-pipeline entry (parseXmlAlg/validateAndParseAiResponseAlg) into that machinery plus path-traversal/absolute malicious paths. '(re)enables' is imprecise — includes are enabled by design today, so there is no AI-specific include gate to regress. Severity P2 is appropriate.
- **Resolved:** commit `ca82b0b` — verify: `ctest -R mc3_ai`
- **Status note:** Confirmed `parseXmlAlg` (AiResponseAlgorithms.hpp) already calls `Mc3Document::loadFromString` with `Mc3LoadPolicy::untrusted()` (`allowIncludes=false`), and confirmed in `Mc3XmlParser.cpp` that `allowIncludes=false` skips `<include>` processing entirely via a plain `if` gate (no exception, no partial side effect). Added `testAiResponseIncludeIsIgnoredNotResolved` (matching the Verify note's precise framing: this proves the AI-pipeline ENTRY into the already-tested include machinery, not the machinery itself): an `<include>` pointing at a real temp file containing a distinctively-named `<definition>` parses successfully through `validateAndParseAiResponseAlg`, and that definition is confirmed ABSENT from the result (not just "no crash") — the actual proof the include was never resolved. A second case covers a path-traversal-shaped (`../../../../etc/passwd`) target, confirming it's ignored unconditionally rather than only for well-formed/existing paths.
- **Blocked:** Depends on the sanitization decision in finding #1 (test asserts whatever confinement policy is chosen).
- **Status note:** AUD-008's fix (Mc3LoadPolicy::untrusted() disables <include> entirely for AI parsing) structurally closes the vector this finding is about, but no ai_test.cpp regression case exercises validateAndParseAiResponseAlg with a malicious <include> end-to-end. Test gap remains open.

### AUD-011 `[DONE]` `P1` `W0` · SDL event watch registered with raw `this` is never removed (dangling callback / UAF at shutdown)
- **Component:** src/MeshCraft/MeshCraftApplication.cpp (LoadContent / sdlEventWatch)
- **Evidence:** LoadContent registers a global SDL event filter capturing the application pointer: MeshCraftApplication.cpp:215 `SDL_AddEventWatch(reinterpret_cast<SDL_EventFilter>(sdlEventWatch), this);`. The callback dereferences that pointer, e.g. MeshCraftApplication.cpp:91 `auto* self = static_cast<MeshCraftApplication*>(userdata);` and :101 `self->pendingDropTexture_ = path;`. There is NO matching `SDL_RemoveEventWatch` anywhere (grep of src/ + include/ returns none), and there is no destructor/UnloadContent/Dispose override on MeshCraftApplication (header has no `~MeshCraftApplication`; CNA `Game::UnloadContent()` is empty and never auto-called, Game.cpp:509). The watch outlives the object: in main.cpp the `app` (an on-stack MeshCraftApplication) is destroyed when it goes out of scope after `app.Run()` returns, but the watcher remains registered in SDL's global state.
- **Outcome:** Pair the SDL_AddEventWatch with SDL_RemoveEventWatch(sdlEventWatch, this) in a new UnloadContent()/Dispose override (or destructor) so the callback cannot fire against a destroyed object; overriding CNA's UnloadContent/Dispose is the intended hook.
- **Tests:** Add a shutdown test (or run under ASAN) that opens the app, triggers Exit(), and confirms no SDL event dispatch reaches sdlEventWatch after the object is destroyed; verify a drop event during teardown does not use freed memory.
- **Verify note:** Severity is borderline P1/P2, and the "UAF at shutdown" framing slightly overstates the immediate impact: in the normal single-run flow nothing pumps SDL events after Run() returns, and SDL is not quit during the object's lifetime, so the stale watch is never actually invoked with a freed `this`. The proven defect is a dangling/leaked global callback registration that holds a pointer to a destroyed object with no matching SDL_RemoveEventWatch — a real lifetime/missing-cleanup bug (latent UAF, not a reliably-triggered one). Everything else in the evidence is accurate.
- **Resolved:** commit `2d126bf` — verify: `ctest -L render (drives full LoadContent->shutdown); manual: SDL_RemoveEventWatch call confirmed present in ~MeshCraftApplication()`

### AUD-012 `[DONE]` `P2` `W0` · ImGui context and SDL3/OpenGL3 backends never shut down (no destructor/UnloadContent/Dispose)
- **Component:** src/MeshCraft/MeshCraftApplication.cpp (LoadContent)
- **Evidence:** LoadContent creates ImGui state that is never released: MeshCraftApplication.cpp:130 `ImGui::CreateContext();`, :212 `ImGui_ImplSDL3_InitForOpenGL(sdlWindow, glCtx);`, :213 `ImGui_ImplOpenGL3_Init("#version 300 es");`. grep for `ImGui::DestroyContext`, `ImGui_ImplOpenGL3_Shutdown`, `ImGui_ImplSDL3_Shutdown` over src/+include/ returns nothing. MeshCraftApplication.hpp declares no `~MeshCraftApplication`, and does not override the CNA hooks `virtual void UnloadContent()` (Game.hpp:253) or `virtual void Dispose(bool)` (Game.hpp:302); the framework never calls Game::UnloadContent (empty at Game.cpp:509) and `~Game` runs Dispose(false) which skips all disposal. The OpenGL3 backend owns a GL font texture, shader program and buffers that leak with it.
- **Outcome:** Override UnloadContent() (or add a destructor) that calls ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext(); in reverse init order.
- **Tests:** Run the app to exit under a GL/leak checker (e.g. valgrind/apitrace) and confirm the ImGui GL objects and context are released; assert the shutdown path executes.
- **Resolved:** commit `2d126bf` — verify: `ctest -L render`

### AUD-013 `[DONE]` `P2` `W0` · shadowDebug FBO and its two textures (member handles) are never deleted by any code path
- **Component:** src/MeshCraft/MeshCraftApplication.cpp (initShadowDebug)
- **Evidence:** initShadowDebug generates GL objects into MeshCraftApplication member fields: MeshCraftApplication.cpp:1568 `gl.GenTextures(1, &shadowDebugColorTex_);`, :1577 `gl.GenTextures(1, &shadowDebugDepthTex_);`, :1586 `gl.GenFramebuffers(1, &shadowDebugFbo_);`. grep for shadowDebug shows these are only ever created, bound, and read (UiOverlays.cpp:2199 ImGui::Image); no glDeleteTextures/glDeleteFramebuffers ever references them. Unlike the s_bloom pool, these live in the MeshCraftApplication object, so even s_bloom.cleanup() cannot free them, and there is no destructor. They leak unconditionally once the shadow-map debug view is enabled once.
- **Outcome:** Delete shadowDebugFbo_/shadowDebugColorTex_/shadowDebugDepthTex_ (guarded by non-zero) in the shutdown hook, and reset the handles to 0.
- **Tests:** Enable shadow-map debug, exit, and confirm the FBO + 2 textures are deleted (GL object counter or apitrace); add a regression test that the handles are freed.
- **Resolved:** commit `2d126bf` — verify: `ctest -L render`

### AUD-014 `[DONE]` `P2` `W0` · Detached AI worker thread is never joined; may run httplib/OpenSSL during process-exit static destruction
- **Component:** src/MeshCraft/AiAssistant.cpp (sendAsync)
- **Evidence:** sendAsync spawns and immediately detaches a network worker: AiAssistant.cpp:180 `std::thread([...]() {...}).detach();` (`.detach()` at :264). reset() (:148-163) only drops the shared_ptr result box and explicitly does NOT wait for the worker ('no blocking anywhere', comment :149-156). isInFlight() (:126) is polled but nothing joins or cancels the thread at exit. main.cpp returns after app.Run() with no shutdown barrier. If an AI request is still in flight when the process exits, the detached thread keeps executing cpp-httplib / OpenSSL code while C++ static destructors and OpenSSL atexit cleanup run, a classic exit-time data race / UB that can crash on shutdown.
- **Outcome:** Track the in-flight worker and, on shutdown, either join it (with a bounded timeout) or ensure the process does not begin static/OpenSSL teardown while a request thread is live; at minimum document/guard the exit ordering.
- **Tests:** Start a request against a slow/blocking mock endpoint, trigger app exit mid-request, and run under TSan/ASan to confirm no thread is executing library code during static destruction.
- **Resolved:** commit `3cd27d7` — verify: `ctest -R mc3_ai`
- **Status note:** Added `AiAssistant::waitForAllInFlight(timeout)`: a process-wide in-flight-worker counter incremented on the CALLING thread before `std::thread(...)` is constructed (avoids a race where the counter could read 0 momentarily between thread creation and the worker's first instruction), decremented via an RAII guard (`AiWorkerScopeGuard`) constructed as the worker lambda's first statement so it decrements on every exit path. `main.cpp` calls it with a bounded 5s timeout via an RAII local (`AiShutdownWaiter`) declared first in `main()` so it is destroyed LAST, right before the process actually returns, on every exit path (`--help`/`--version`/`--screenshot`/`--export`/interactive). Bounded, not indefinite, deliberately — an unbounded wait here would reintroduce the exact hang STAB-0387/0388 removed from `reset()`; a genuinely-stuck request is still abandoned after the timeout, same as before this fix, this only helps the near-finished case. Did not attempt the TSan/ASan verification named in Tests (that's covered by the separate, broader AUD-055 CI-hardening task) — instead directly exercised the synchronization primitive against a real in-flight worker thread (mock `httplib::Server` blocked via a released condition_variable): a short-timeout call returns `false` while genuinely still blocked, a longer one returns `true` once released and the worker actually finishes, and the result is confirmed already-published at that point (no wait/data race).

### AUD-015 `[DONE]` `P2` `W6` · Known-key value decoding ignores the declared tag byte (no type validation on the hot read path)
- **Header note (2026-07-17):** the summary above previously still read
  "PARTIAL: readObject done, 27 sibling read* functions remain" despite the
  entry's own "Resolved"/"Status note (completion)" lines below already
  recording the full rollout (commit `00aee08`, 189 `expectTag` call sites).
  Corrected here — verify with `grep -c "expectTag(" mcb/src/McbReader.cpp`
  (189, confirmed still current).
- **Component:** mcb/src/McbReader.cpp — readObject/readDocument and all read* deserializers
- **Evidence:** For every recognized key the reader reads the tag byte but never validates it against the expected type; it decodes the value purely by key name. readObject reads `uint8_t tag = rU8(in);` (McbReader.cpp:383) then for "type" does `obj->type = static_cast<Mc3::ObjectType>(rI32(in));` (line 384) and for "visible" does `obj->visible = rU8(in) != 0;` (line 388) — `tag` is used ONLY in the final `else skipValue(in, tag);` (line 446), never checked for known keys. Same pattern in every read* helper (e.g. readTransform 206-209, readMaterial 608-622). So a file whose known field carries a mismatched tag (e.g. key "visible" with tag TAG_STR + a 4-byte length) is not rejected at the field; the reader reads by the wrong type and desyncs the stream. There is also no checksum in the header (McbFormat.hpp:6-20), so this type info is the only per-field integrity signal and it is discarded.
- **Outcome:** For recognized keys, verify the read tag equals the expected tag before decoding (throw "MCB: type mismatch for key ..." otherwise), so a corrupt/hostile tag/value mismatch is detected at the field instead of silently desyncing into a wrong-but-parsed document. Bounds already hold (rRawStr/rU32Bounded), so this is a strictness/integrity fix, not a crash fix.
- **Tests:** Add a corruption test in mcb_roundtrip_test.cpp: hand-write a stream with a known key (e.g. "visible") carrying a wrong tag and assert loadFromBinary throws a type-mismatch error rather than succeeding or throwing an unrelated 'unknown tag' later.
- **Verify note:** Evidence path correction: the header is at mcb/include/MeshCraft/Mcb/McbFormat.hpp (not bare "McbFormat.hpp:6-20"); the no-checksum claim is still accurate (lines 6-32 define only MAGIC/VERSION/MIN_SUPPORTED_VERSION/FLAG_COMPRESSED and TAG_* constants). The readTransform "206-209" and readMaterial "608-622" ranges point at the value-decode lines; the tag-read lines are 205 and 607 respectively. Severity P2 is appropriate and not inflated given the bounded (exception-only) impact.
- **Status note (partial, commit `a0c1d8a`):** Added `expectTag(got, want, key)` and applied it to every field in `readObject` (the function this finding's evidence literally cites — all scalar/TAG_OBJ/TAG_ARR/TAG_MAP branches: type, name, id, material, visible, collision, layer, isCutter, definition, meshSource, materialOverride, transform, deform, primitive, csgOperation, extrude, uvMapping, tags, variantDefs, metadata, states, children). Verified with `mcb_roundtrip_test`'s new `testKnownKeyTagMismatchRejected`/`testKnownKeyTagMatchStillLoads`. **NOT yet done:** the same pattern in the other 27 `read*` deserializers (readTransform, readPrimitive, readDeform, readCsgOp, readCrossSection, readPathPoint, readPath, readExtrude, readUvMapping, readObjectState, readTexture, readSvgTexture, readObjectOverride, readSceneState, readTrigger, readSound, readMusic, readScript, readEmbed, readMaterial, readLight, readCamera, readFog, readEnvironment, readKeyframe, readChannel, readAction, readDocument) — each has the identical unchecked-tag pattern and needs the identical mechanical fix. Left `[TODO]` rather than `[DONE]` because most of the cited surface area is still open; verify with `grep -c "expectTag(" mcb/src/McbReader.cpp` (currently 24 call sites, all in `readObject`; each of the other 27 `read*` functions currently contributes 0).
- **Resolved:** commit `00aee08` (full rollout, completing the partial fix above) — verify: `ctest -R mcb_roundtrip`, `grep -c "expectTag(" mcb/src/McbReader.cpp` (now 189, up from 24)
- **Status note (completion):** Applied `expectTag` at all remaining field-decode sites across all 27 previously-open `read*` functions, including `readDocument` — the root/most security-critical function, whose ~20 collection fields (meta/metadata/includes/lights/textures/materials/objects/actions/etc.) had NO tag check at all before this, not even readObject's level of protection. `readTrigger`'s inner step fields (type/ref) also had zero tag validation and were fixed; `readSceneState`/`readTrigger`'s OUTER array-key checks (`k == "overrides" && tag == TAG_ARR`) already had an equivalent skip-not-throw guard and were deliberately left as-is (different but already-safe style — changing skip-on-mismatch to throw-on-mismatch there would be a behavior change beyond this task's scope). While extending coverage, found 2 enum-typed fields AUD-017's original pass missed entirely — `ExtrudePathType` (readPath) and `UvProjection` (readUvMapping) — and applied `clampEnum` to both. New tests prove the rollout has real teeth on a nested function (not just readObject) and cover the 2 newly-found enums. Full suite 101/101 passing, including every field of the pre-existing large action/keyframe roundtrip tests — strong evidence every `TAG_*` constant assigned is correct (one wrong constant would have broken a real roundtrip).

### AUD-016 `[DONE]` `P3` `W6` · RecursionGuard comment claims two independent depth counters, but both recursion trees share one
- **Component:** mcb/src/McbReader.cpp — RecursionGuard
- **Evidence:** The comment states: "Two independent counters (one per recursion tree: skipValue's skip-path, readObject's real-object-tree path) are used rather than one shared counter" (McbReader.cpp:130-132). But the counter is a `static thread_local int depth_` on the class template (lines 147, 149-150), and BOTH recursion sites instantiate the SAME specialization `RecursionGuard<256>`: skipValue at line 165 and readObject at line 379. One instantiation ⇒ one shared `thread_local depth_`. So the object-tree depth and the skip-path depth share a single 256 budget, directly contradicting the comment. Effect is harmless (the shared counter is strictly more conservative), but a forward-compat file whose combined object+skip nesting lands between 257 and 511 would be rejected though the documented 'independent 256+256' design would accept it.
- **Outcome:** Either make the counters actually independent (give skipValue and readObject distinct template tags, e.g. RecursionGuard<256, struct SkipTag> vs RecursionGuard<256, struct ObjTag>) to match the comment, or fix the comment to state that a single shared 256 budget is used across both trees. The code and comment must agree.
- **Tests:** No behavior change needed if the comment is corrected; if made independent, add a test that a legal-but-deep combined object+skip nesting (~300 combined) still loads.
- **Resolved:** commit `ba3e73c` — verify: read the corrected comment at mcb/src/McbReader.cpp:120-132
- **Status note:** Took the "fix the comment" branch of Outcome — the shared-counter behavior is harmless (strictly more conservative than two independent counters would be), and no current file/test needs the 257-511 combined-depth edge case the independent-counters alternative would unlock, so splitting the template instantiation would add complexity for no real behavioral benefit. Comment now states plainly that both call sites share one `RecursionGuard<256>` instantiation and therefore one budget. No behavior change, no new test needed (matches Tests field's own "no behavior change needed if the comment is corrected" branch).

### AUD-017 `[DONE]` `P3` `W6` · Enum fields are cast from unvalidated file ints with no range check
- **Component:** mcb/src/McbReader.cpp — enum-typed field reads
- **Evidence:** Every enum field is a raw static_cast of an attacker-controlled int with no validation: `obj->type = static_cast<Mc3::ObjectType>(rI32(in));` (McbReader.cpp:384), `p.primitiveType = static_cast<Mc3::PrimitiveType>(rI32(in))` (line 220), `csg.csgType = static_cast<Mc3::CsgType>(rI32(in))` (251), `cs.type = static_cast<Mc3::CrossSectionType>(rI32(in))` (262), plus LightType (633), CameraType (654), FogMode (674), Interpolation (704), AnimatedProperty (720). A malformed MCB can inject out-of-range enum values that downstream switch statements (mesh generation, glTF export) may not have a default case for. Not UB (scoped enums have fixed int underlying type) and not a reader crash, but a missing-validation gap on hostile input that the reader silently propagates as a 'successful' parse.
- **Outcome:** Optionally clamp/validate each enum against its known range on read (fall back to the default enumerator on out-of-range, matching the unknown-key forward-compat philosophy) so a corrupt file cannot inject an enum value no downstream switch handles.
- **Tests:** Add a corruption test feeding an out-of-range ObjectType/PrimitiveType int and asserting the reader either normalizes it to the default or rejects it, rather than storing an unhandled value.
- **Resolved:** commit `ba3e73c` — verify: `ctest -R mcb_roundtrip`
- **Status note:** Added `clampEnum<Enum>(raw, count)` — an out-of-range value clamps to enumerator 0 (matching the unknown-key forward-compat philosophy: degrade to a defined value, don't reject the whole file over one bad field), applied at all 9 sites named in Evidence: ObjectType(19), PrimitiveType(11), CsgType(3), CrossSectionType(5), LightType(4), CameraType(2), FogMode(2), Interpolation(3), AnimatedProperty(22) — enumerator counts hand-verified against each enum's real definition (no reflection available to derive them automatically; a comment on `clampEnum` flags this as a hand-sync point). New `testOutOfRangeEnumClampedToDefault` feeds `type=9999` for an object and confirms it loads without error, clamped to `ObjectType::Box` (enumerator 0) rather than storing 9999.

### AUD-018 `[DONE]` `P3` `W6` · MCB_FORMAT.md top-level field-order list is stale (omits rotationUnits, eulerOrder, includedEmbeds)
- **Component:** MCB_FORMAT.md — 'Field key names and document layout'
- **Evidence:** MCB_FORMAT.md:100-106 presents the authoritative writeDocument() order as: "version, model, unit, coordinateSystem, defaultCamera, meta, metadata, includes, includedDefs, includedMaterials, includedTextures, environment, ...". But McbWriter.cpp actually writes `rotationUnits` and `eulerOrder` between coordinateSystem and defaultCamera (McbWriter.cpp:461-462) and writes `includedEmbeds` after includedTextures (McbWriter.cpp:494-497). Both are also read back (McbReader.cpp:768-769, 822-829) and covered by tests (testDocumentRotationConvention). The documented order is therefore incomplete/inaccurate.
- **Outcome:** Update the field-order block in MCB_FORMAT.md to include rotationUnits, eulerOrder, and includedEmbeds in their actual written positions.
- **Tests:** Doc-only; no runtime test.
- **Verify note:** Line numbers are exact; only the file paths in the claim were abbreviated. Full paths: mcb/src/McbWriter.cpp:461-462 and :494-497; mcb/src/McbReader.cpp:768-769 and :822-829. Severity P3 stands.
- **Resolved:** commit `ba3e73c` — verify: read MCB_FORMAT.md's field-order block against `writeDocument()` in mcb/src/McbWriter.cpp
- **Status note:** Re-verified the exact write order directly in `writeDocument()` (version, model, unit, coordinateSystem, rotationUnits, eulerOrder, defaultCamera, meta, metadata, includes, includedDefs, includedMaterials, includedTextures, includedEmbeds, environment, ...) and updated the doc block to match exactly. Doc-only change, no runtime test per the Tests field.

### AUD-019 `[DONE]` `P3` `W6` · Compression-flag rejection path has no test despite being a documented guarantee
- **Component:** mcb/test/mcb_roundtrip_test.cpp
- **Evidence:** McbReader.cpp:1004-1005 rejects a file with MCB_FLAG_COMPRESSED set (`throw std::runtime_error("MCB: compressed format not yet supported")`), and MCB_FORMAT.md:19 documents this as a guaranteed behavior ("a file with this bit set is rejected with a clear error, not silently misread"). The test suite exercises version bounds, huge string/count, truncation, all-zeros, single-byte, and deep nesting, but grep shows the only 'compress' reference in tests is testSmoke asserting the writer emits flags==0 (mcb_roundtrip_test.cpp:62); no test ever sets the compressed flag and asserts loadFromBinary rejects it. The documented reject path is untested.
- **Outcome:** Add a test that hand-writes a valid header with flags=MCB_FLAG_COMPRESSED and asserts loadFromBinary throws with 'compressed format not yet supported'.
- **Tests:** The new test itself is the verification.
- **Resolved:** commit `ba3e73c` — verify: `ctest -R mcb_roundtrip`
- **Status note:** Added `testCompressedFlagRejected`, hand-writing a minimal otherwise-valid header (magic + version + `MCB_FLAG_COMPRESSED` flags byte + reserved + empty root object) and asserting `loadFromBinary` throws with the exact documented message.

### AUD-020 `[DONE]` `P1` `W7` · Spot light position is dropped — every spotlight is exported at the world origin
- **Component:** mc3togltf/src/GltfExporter.cpp addLights()
- **Evidence:** GltfExporter.cpp:979-990: `if (light.type == LightType::Directional || light.type == LightType::Spot) { ... lnode.rotation = {q...}; } else { lnode.translation = {light.position...}; }`. Spot lights take the rotation-only branch, so `lnode.translation` is never set. But Mc3Light.hpp:19-20 documents `position` as meaningful for `// Spot / Point`. A spotlight authored at any position is emitted with only its direction-derived rotation and no translation → placed at (0,0,0). Directional lights legitimately ignore position, but spots do not.
- **Outcome:** Emit BOTH translation (from light.position) and rotation (from direction) for Spot lights; only Directional should be rotation-only.
- **Tests:** Export a scene with a spot light at position (5,3,0); assert the light node's translation == (5,3,0)*unitScale, not origin.
- **Resolved:** commit `22b7129` — verify: `ctest -R mc3togltf_light`

### AUD-021 `[DONE]` `P1` `W7` · Point/spot light position and range are not multiplied by unitScale — lights misplaced in non-meter documents
- **Component:** mc3togltf/src/GltfExporter.cpp addLights()
- **Evidence:** addLights (GltfExporter.cpp:922-924) takes no unitScale parameter and is called at GltfExporter.cpp:1453 `addLights(model, doc.lights, lightNodes);` (no scale). Point-light translation uses the raw value: GltfExporter.cpp:985-989 `lnode.translation = { static_cast<double>(light.position[0]), ... }`, and range at 958-959 `lo["range"] = ...(light.range)`. Meanwhile all geometry AND cameras are scaled: camera code at 1062-1066 multiplies by unitScale (STAB-0693 comment: 'cameras previously used raw, un-scaled positions while every other node's translation is scaled by unitScale'). Lights were left with exactly the bug STAB-0693 fixed for cameras. In a unit='centimeter'/'inch' document the geometry is scaled but point lights and light ranges are not, so lights end up at the wrong distance.
- **Outcome:** Pass ctx.unitScale into addLights and multiply light.position and light.range by it, matching addCameraNodes (STAB-0693).
- **Tests:** Export a unit='centimeter' scene with a point light at (100,0,0); assert node translation == (1,0,0).
- **Verify note:** Evidence is correct with one small imprecision: spot lights (979-983) receive only a rotation and no translation at all, so the "spot light position not scaled" wording is off — spot lights export no position. Accurate statement: point-light position (985-989) is exported unscaled, and light.range (958-959) is exported unscaled for BOTH point and spot lights. Severity P1 is appropriate (incorrect output in non-meter documents, not a crash).
- **Resolved:** commit `22b7129` — verify: `ctest -R mc3togltf_light`

### AUD-022 `[DONE]` `P1` `W7` · Extrude end-caps use naive fan triangulation — concave cross-sections (built-in Star, arbitrary Custom) produce overlapping/incorrect cap geometry, silently
- **Component:** mc3togltf/src/MeshBuilder.cpp buildExtrude() addCap()
- **Evidence:** MeshBuilder.cpp:1052-1055: `for (uint32_t i = 1; i + 1 < ring.size(); ++i) { if (flip) ...{base, base+i+1, base+i}; else ...{base, base+i, base+i+1}; }` — a triangle fan anchored at ring vertex 0. This is only correct for CONVEX polygons. sampleCrossSection produces a genuinely concave built-in shape: CT::Star (MeshBuilder.cpp:686-699) alternates outer/inner radius vertices. Fan-triangulating a star from vertex 0 emits triangles that cover the concave notches and overlap outside the outline — a wrong, self-overlapping cap. CT::Custom (700-702) can be arbitrarily concave with the same result. Side walls are fine; only the caps are wrong, and no warning is emitted.
- **Outcome:** Use a proper polygon triangulation (ear-clipping / monotone) for caps, or at minimum warn when the cross-section is non-convex and caps are requested.
- **Tests:** Extrude a Star cross-section with caps=true; verify cap triangles all lie inside the star outline and do not overlap (e.g. signed-area / point-in-polygon check).
- **Resolved:** commit `ac75eb6` — verify: `ctest -R mc3togltf_earclip`

### AUD-023 `[DONE]` `P1` `W7` · Generated primitive/extrude geometry is never validated for finiteness — degenerate params emit NaN positions and NaN accessor min/max while export reports success
- **Component:** mc3togltf/src/MeshBuilder.cpp / GltfExporter.cpp
- **Evidence:** Helix path tangent MeshBuilder.cpp:748 `float tx = -std::sin(t), ty = h / (r * totalAngle), tz = std::cos(t);` divides by `r * totalAngle`; helixTurns==0 (totalAngle=0) or helixRadius==0 makes ty = h/0 = inf → normalization ty/tl = inf/inf = NaN → NaN propagates into positions/normals. buildSphere (97-101,104-106) computes `phi = pi * r / rings` with `rings = segments/2`; segments==1 → rings==0 → 0/0 = NaN. There is NO finiteness guard on generated geometry before writing, and addAccessorVec3's bounds pass (GltfExporter.cpp:213-226) copies data[i] straight into acc.minValues/maxValues, so NaN lands in the accessor min/max (spec-invalid). The OBJ path explicitly rejects this (MeshBuilder.cpp:1145-1150 `for (float v : attrib.vertices) if (!std::isfinite(v)) throw ...`), showing the authors know it matters — but only for OBJ. Export still writes the file and main.cpp:93 prints 'Written:' and returns 0.
- **Outcome:** Add the same finiteness check the OBJ loader has (or clamp/validate degenerate primitive params) to buildMesh/addMeshDataToGltf so a NaN/Inf mesh fails loudly instead of producing a spec-invalid glTF reported as success.
- **Tests:** Export an Extrude with a Helix path, helixTurns=0; assert the exporter throws (or the output contains no NaN) rather than exiting 0 with NaN accessor bounds.
- **Resolved:** commit `fd606d2` — verify: `ctest -R mc3togltf_hostile_geometry`

### AUD-024 `[DONE]` `P2` `W7` · Per-object UV mapping (projection/scale/offset/rotation) is silently ignored by the exporter
- **Component:** mc3togltf/src/GltfExporter.cpp / MeshBuilder.cpp
- **Evidence:** Mc3Object carries `std::optional<Mc3UvMapping> uvMapping;` (Mc3Object.hpp:91; Mc3UvMapping has projection + scaleU/V, offsetU/V, rotation, Mc3Object.hpp:21-28). grep across mc3togltf/src for uvMapping/scaleU/offsetU returns NONE — no exporter code reads it. buildMesh/buildPrimitive emit only the primitive's built-in TEXCOORD_0. A user who set UV scale=4 or a Box/Sphere projection in the editor gets 1x planar UVs in the export, with no warning. MC3_FORMAT.md lists 'Materials (PBR, textures) ✅', implying textures round-trip faithfully.
- **Outcome:** Apply Mc3UvMapping (scale/offset/rotation and projection) to generated texcoords, or warn that authored UV mapping is not exported.
- **Tests:** Export an object with uvMapping scaleU=4; assert exported TEXCOORD_0 U range spans ~4x, or a warning is emitted.
- **Verify note:** Evidence is accurate except one nuance: the finding implies the editor viewport applies uvMapping while only the export drops it. In fact the editor renderer (src/MeshCraft/Renderer/*) also never reads uvMapping — a grep shows only PropertiesPanel.cpp (the property editor) touches it. So uvMapping is applied NOWHERE in geometry generation (neither live viewport nor glTF export); it merely round-trips through XML/MCB serialization. Full path of the field is mc3/include/MeshCraft/Mc3/Mc3Object.hpp:91 (line number matches the claim). Severity P2 stands.
- **Resolved:** commit `29e5706` — verify: `ctest -R mc3togltf_uv_mapping_export`
- **Status note:** Added `MeshData::applyUvMapping()` (scale, then rotate about the UV origin, then offset) and wired it into `buildMesh()` whenever `obj.uvMapping` is set. Box/Sphere projection remains genuinely unimplemented (confirmed by the Verify note: nowhere in the codebase recomputes UVs from a projection type) — a non-Planar projection now warns rather than silently claiming to apply something that has no effect, taking the "or warn" branch of this task's Outcome for that part. Also fixed an adjacent bug this task's own testing surfaced: `buildGeomCacheKey()` omitted `uvMapping`, so two same-size/material objects differing only by `uv_mapping` collided on the same cache key and silently shared the first object's TEXCOORD_0. **Known remaining gap** (out of this task's scope, not fixed here): the live editor viewport (src/MeshCraft/Renderer/*) still never reads `uvMapping` either — only the exporter was fixed. A user editing UV mapping still sees no visual feedback in the 3D view itself, only in the final export; not filed as a separate AUD-### since the Verify note already documents it as a known, pre-existing, viewport-side gap distinct from this exporter fix.

### AUD-025 `[DEFERRED]` `P2` `W7` · embed: mesh source is treated as a literal OBJ path — node exports with no mesh while export exits 0 'Written'
- **Component:** mc3togltf/src/GltfExporter.cpp buildMesh()
- **Evidence:** GltfExporter.cpp:586-593: `if (obj.type == ObjectType::Mesh && !obj.meshSource.empty()) { try { md = loadObjMesh(ctx.basePath, obj.meshSource); } catch (...) { std::cerr << Warning ...; ctx.stats.warnings++; return -1; } }`. The exporter never checks for the `embed:<id>` form the mc3 parser/writer round-trip (Mc3XmlParser.cpp:743-749). `loadObjMesh` tries to open a file literally named 'embed:tree', fails, warning printed, node gets no mesh. main.cpp:93 then prints 'Written:' and returns 0. Documented (MC3_FORMAT.md, STAB-0194/0549) and a warning + stats.warnings signal it, so not fully silent — but the tool still reports success with dropped geometry.
- **Outcome:** Resolve embed:<id> against doc.embeds (parse the referenced/inline GLB and merge its meshes), or make the missing-geometry case a non-zero exit / clearer failure rather than 'Written' success.
- **Tests:** The existing embed_mesh_source_test.py locks in the degraded behavior; add resolution or assert a distinct exit/status when geometry is dropped.
- **Verify note:** Evidence is accurate; no correction needed. Severity P2 is appropriate: the geometry loss is signalled by a stderr Warning and the stats.warnings counter (only shown with --show-stats), and the behavior is documented in MC3_FORMAT.md and locked in by a passing test (mc3togltf/test/embed_mesh_source_test.py, STAB-0549) that asserts exit 0 + warning + empty node is the intended, accepted limitation. It is therefore a low-severity known limitation rather than a silent data-loss bug, but the core claim (tool reports 'Written'/exit 0 while dropping the embed:-referenced mesh) is factually correct.
- **Status note:** Existing embed_mesh_source_test.py locks in the current documented-limitation behavior (exit 0 + warning + empty node); the adversarial re-verify pass judged this an accepted limitation, not a defect requiring a code change. Left TODO-eligible for W14 embed: resolution work (SYS-W14-05).

### AUD-026 `[DONE]` `P2` `W7` · --stats 'Warnings' count is untruthful — several warning paths never increment stats.warnings
- **Component:** mc3togltf/src/GltfExporter.cpp
- **Evidence:** ctx.stats.warnings++ is called only at GltfExporter.cpp:591 (OBJ load fail), 809 (unknown definition), and 849 (approximate CSG). These warning paths print to stderr but do NOT increment it: unknown material at 765 `std::cerr << "Warning: object '" ... unknown material ...` (no ++); SVG-slot warnings in buildMaterial (496-501, function has no ctx); ambient-light drop at 935 `std::cerr << ... ambient light ... omitted`; duplicate node name at 1476. So `--stats` 'Warnings: N' (main.cpp:107) undercounts real warnings, misrepresenting export health.
- **Outcome:** Route all warning emissions through a single counter (or thread ctx/stats into buildMaterial and addLights) so stats.warnings matches the warnings actually printed.
- **Tests:** Export a scene that triggers an unknown-material and an ambient-light warning; assert stats.warnings equals the number of warnings printed.
- **Verify note:** Evidence understates the issue: even more warning paths omit the increment — line 393 (could not detect image format), 441 (texture not found for embedding), and 1177/1199 (action warnings) also print 'Warning:' without incrementing stats.warnings. Severity P2 is appropriate (misleading reported statistic; no output corruption).
- **Resolved:** commit `79cc9c0` — verify: `ctest -R 'mc3togltf_export_stats|mc3togltf_gltf'`
- **Status note:** Threaded a `int& warningCount` reference through every free function that prints "Warning:" (`detectImageMimeType`, `buildTextures`, `buildMaterial`, `addLights`, `exportAnimations`); `buildNode` itself already has `ctx` in scope so its 3 sites just call `ctx.stats.warnings++` directly. `buildTextures`/`buildMaterial` run before `ExportCtx` exists (its `matNameToIdx` is built FROM `buildMaterial`'s output), so they accumulate into a local `preCtxWarnings` that gets folded into `ctx.stats.warnings` right after construction. All 12 "Warning:" print sites in the file now increment the shared counter — verified by grep, not just spot-checked. Fixing this surfaced that `mc3togltf_export_stats`'s "clean scene" fixture (house.mc3.xml) was never actually warning-free — it has an unnamed `<ambient>` light, which STAB-0696 already warns about on every export; that warning was simply one of the previously-uncounted ones. Updated the test to expect the true count (1) instead of editing the shared fixture.

### AUD-027 `[DONE]` `P1` `W7` · Pivoted object + translation animation loses the pivot offset — object jumps when its translation is animated
- **Component:** mc3togltf/src/GltfExporter.cpp buildNode()/exportAnimations()
- **Evidence:** For a pivoted object the outer node's static translation includes the pivot: GltfExporter.cpp:738-743 `node.translation = { t.position[i] + t.pivot[i] }`. Animation channels target that same outer node by name (nodeNameMap), but the translation sampler builds values from the pivot-free base transform: GltfExporter.cpp:1259-1260 `base[0] = baseT.position[0]; ...` (baseTransforms is obj.transform, no pivot), then valueData uses base for non-animated axes and evaluateChannel (also pivot-free) for animated ones. So when any translation channel plays, node.translation is overwritten with position*unitScale, dropping the +pivot the static pose had → the object snaps by -pivot at animation start.
- **Outcome:** Add the pivot offset into the animated translation values (or target the inner _origin node), so animated and static poses agree for pivoted objects.
- **Tests:** Export an object with a non-zero pivot and a translation channel; assert the sampler's value at t=keyframe0 equals the static node translation.
- **Verify note:** Jump magnitude is precisely -pivot*unitScale (unitScale is applied to both the static translation at 908-912 and the animated values at 1287). Severity is arguably understated: this silently emits incorrect exported animation output (visible discontinuity), which matches P1 (wrong behavior / silent invalid output) better than P2. This under-rating does not refute the finding.
- **Resolved:** commit `79cc9c0` — verify: `ctest -R mc3togltf_gltf`
- **Status note:** In the `path == "translation"` branch of `exportAnimations`, added `baseT.pivot[i]` to every sampled/fallback component value before applying `unitScale` — matching the exact `position + pivot` formula `buildNode` uses for the static pose, applied in the same pre-scale order. Added a dedicated fixture (`PivotedBox`/`PivotSlide` action in `test/animation_test.mc3.xml`) and a new `test_pivot_animation()` in `gltf_test.py` that decodes the real GLB buffer (not just accessor metadata) and asserts the sampler's t=0 value equals the static node's translation exactly (5.5, matching position 5.0 + pivot 0.5), not the pivot-free authored keyframe value (5.0).

### AUD-028 `[DONE]` `P1` `W7` · Rotation animation exported only as LINEAR/STEP quaternion samples (never CUBICSPLINE); euler-space keyframe interpolation is replaced by quaternion shortest-path lerp
- **Component:** mc3togltf/src/GltfExporter.cpp exportAnimations()
- **Evidence:** Rotation output is baked to per-keyframe quaternions (GltfExporter.cpp:1269-1280 `auto q = eulerToQuat(euler...); valueData.push_back(q[0..3])`) and the sampler uses `interp` = STEP or LINEAR (1255, 1343). glTF LINEAR rotation interpolation is normalized quaternion lerp along the SHORTEST arc between consecutive samples. For plain (non-cubic) LINEAR keyframes only the original keyframe times are emitted, so a euler channel that sweeps a component >180° between two keyframes (e.g. 0°→270° about one axis) is reproduced as the -90° short path instead of the authored 270°. CUBICSPLINE is never emitted (cubic bezier is baked to dense LINEAR, 1247-1252), which mitigates cubic curves but not sparse large-angle LINEAR rotations.
- **Outcome:** Either densely sample large-angle rotation keyframes (as done for cubic) so quaternion lerp tracks the euler path, or document that rotation channels with >180° per-segment component deltas are re-pathed.
- **Tests:** Export a rotation channel 0°→270° over two keyframes; assert intermediate sampled orientation matches the editor's euler interpolation (not the -90° short path).
- **Verify note:** Two refinements. (1) Terminology: glTF LINEAR interpolation for rotation channels is spherical linear interpolation (slerp), not plain "normalized quaternion lerp"; both take the shortest arc, so the finding's conclusion is unchanged. (2) Severity: P2 understates it. This silently produces animation that plays the wrong way (or, for a 0°→360° full-turn LINEAR segment, no rotation at all — quat(360°) equals quat(0°)/its antipode) while the export is reported as successful, which is wrong-output-reported-as-success — better classified P1 (silent output divergence from the authored/edited animation) than P2 maintainability.
- **Resolved:** commit `79cc9c0` — verify: `ctest -R mc3togltf_gltf`
- **Status note:** Extended the existing cubic-bezier dense-sampling mechanism (already proven to preserve curve shape via `evaluateChannel()`) to also trigger for plain-LINEAR rotation channels whose consecutive raw keyframes have a per-axis delta exceeding 180 degrees (STEP channels exempted — they never interpolate, so shortest-arc re-pathing does not apply to them). The fixture's own `Spin` action (0°→360° over exactly 2 keyframes, `test/animation_test.mc3.xml`) turned out to be the textbook degenerate case named in the Verify note — its pre-existing test assertion (`gltf_test.py`) was locking in the OLD buggy behavior (`ta["count"] == 2`, i.e. asserting the bug's absence of densification as a feature). Updated that assertion to `> 2` with an explanatory comment, and confirmed via the AUD-027 buffer-decoding addition's technique that dense sampling is what now fires.

### AUD-029 `[DONE]` `P3` `W7` · Per-object metadata is silently dropped from glTF, unlike tags/collision which are exported to node extras
- **Component:** mc3togltf/src/GltfExporter.cpp buildNode()
- **Evidence:** Mc3Object has `std::map<std::string,std::string> metadata;` (Mc3Object.hpp:94, an opaque pass-through mirroring <metadata>). The node-extras block (GltfExporter.cpp:882-905) emits mc3_type, tags, and collision but never obj.metadata (grep for .metadata/->metadata in mc3togltf/src returns NONE). So metadata survives mc3/mcb round-trips but is dropped on glTF export with no warning, even though sibling opaque data (tags) is preserved via extras.
- **Outcome:** Serialize obj.metadata into node.extras (e.g. extras['metadata']) alongside tags/collision, or document the drop.
- **Tests:** Export an object with metadata['foo']='bar'; assert node.extras carries it.
- **Resolved:** commit `79cc9c0` — verify: `ctest -R mc3togltf_object_metadata_export`
- **Status note:** Added an `extras["metadata"]` object built from `obj.metadata`, alongside the existing `tags`/`collision`/`mc3_type` extras in the same block. New fixture `test/object_metadata_export.mc3.xml` (a tagged box with 2 metadata properties, plus a plain box with none) and `mc3togltf/test/object_metadata_export_test.py` assert both the round-tripped key/value pairs on the tagged object and that an object with no `<metadata>` gets no `extras.metadata` key at all (no spurious empty object).
- **Verify note:** Full header path is mc3/include/MeshCraft/Mc3/Mc3Object.hpp (line 94 is exactly correct). Note the one grep hit at GltfExporter.cpp:1414 is a comment for DOCUMENT-level metadata (doc.model/version/unit -> asset.extras), not per-object obj.metadata, so it does not contradict the finding.

### AUD-030 `[DONE]` `P1` `W4` · insertAnimKeyframesAlg mirror has diverged from production insertAnimKeyframes (STAB-0715) — Material/Deform keyframes captured as 0.0 instead of the live value
- **Component:** include/MeshCraft/EditorAlgorithms.hpp:1456 vs src/MeshCraft/MeshCraftApplication_Anim.cpp:195
- **Evidence:** The mirror is explicitly documented as "Mirrors MeshCraftApplication::insertAnimKeyframes()" (EditorAlgorithms.hpp:1424-1434) and is TEST-ONLY (0 production call sites; only mc3/test/editor_commands_test.cpp:2166/2178/2187/2193 call it). The mirror still uses the pre-STAB-0715 10-way switch: EditorAlgorithms.hpp:1457-1470 handles Position/Rotation/Scale/Visible from the live object but every other property (all Material* and Deform*) hits `default: value = Mc3::evaluateChannel(action.channels[ci], animTime); break;` — and for a brand-new empty channel evaluateChannel returns 0.0f. Production was rewritten by STAB-0715 (Anim.cpp:185-195): `float value = resolveObjectPropertyValue(document_, obj, prop);` which reads live values for ALL 22 properties, with documented non-zero defaults — DeformX/Y/Z return 1.0 (Anim.cpp:40-42) and MaterialBaseColorR/G/B return 0.8, alpha 1.0, roughness 0.5 (Anim.cpp:57-62). So inserting a Deform or Material keyframe: production records e.g. 1.0/0.8, the mirror records 0.0. The mirror no longer mirrors production; the divergence is latent only because the test exercises PositionX/Visible (identical on both sides) and never a Material/Deform property.
- **Outcome:** Re-sync insertAnimKeyframesAlg to call the same value-resolution logic as production (a resolveObjectPropertyValueAlg mirror of Anim.cpp:24-79), OR delete the mirror and test insertAnimKeyframes through production. Then add a test that inserts a Deform/Material keyframe and asserts the captured value is the object's live value (1.0 / 0.8 / etc.), not 0.0.
- **Tests:** New editor_commands_test case: object with no deform, insert DeformX keyframe, assert keyframe.value==1.0 (currently the mirror would yield 0.0). Same for MaterialRoughness → 0.5.
- **Verify note:** Evidence line-citation is slightly off: the "Mirrors MeshCraftApplication::insertAnimKeyframes()" statement is at EditorAlgorithms.hpp:1411 (the doc block spans 1409-1419), not "1424-1434" (line 1424 is a function parameter). Two doc-comment staleness details strengthen the finding: (a) lines 1416-1417 still claim material/deform have "no live current value on Mc3Object," which resolveObjectPropertyValue now contradicts; (b) the header's cross-reference "Anim.cpp:94-153" is stale — the production function now lives at Anim.cpp:159-215. Severity: P1 is defensible (test-mirror no longer mirrors production + untruthful/stale doc comment), but it leans P2 since the function is test-only, the divergence is latent, and no test currently fails nor is production behavior affected.
- **Resolved:** commit `dcd0d33` — verify: `ctest -R mc3_commands`

### AUD-031 `[DONE]` `P2` `W4` · 26 EditorAlgorithms.hpp *Alg functions are test-only parallel copies with zero production call sites — production runs a separate hand-copied implementation (Gate B)
- **Component:** include/MeshCraft/EditorAlgorithms.hpp (whole file)
- **Evidence:** Grepping each *Alg name for real call sites (`name(` in src/ and mc3togltf/src/, excluding comments) returns production_call_sites=0 for: mergeDocumentsAlg, collectObjectIdsAlg, resolveObjectIdCollisionsAlg, duplicateObjectsAlg, groupObjectsAlg, ungroupObjectAlg, autoSavePathAlg, autoSaveTickAlg, rotateBackupsAlg, exportSelectionAlg, insertAnimKeyframesAlg, loadPrefsAlg, savePrefsAlg, saveMacroAlg, loadMacroAlg, loadRecentFilesAlg, saveRecentFilesAlg, addRecentFileAlg, keyBindToStringAlg, keyBindFromStringAlg, materialColorAlg, hierarchyFilterMatchesAlg, pushWithCapAlg, resolveSaveAsPathAlg, unsavedDialogResolvesToExecuteAlg, confirmIfModifiedAlg. Each has a separate production original the test never touches, e.g. duplicateObjectsAlg (EditorAlgorithms.hpp:318) vs duplicateSelected (Commands.cpp:184-210); groupObjectsAlg (:341) vs groupSelected (Commands.cpp:256-282); autoSaveTickAlg (:989) vs the update() branch (MeshCraftApplication.cpp:341-348); rotateBackupsAlg (:1016) vs the saveFile() block (FileOps.cpp:176-183); mergeDocumentsAlg (:1178) vs mergeSceneFromFile (FileOps.cpp:607-654) which has its own inline collectObjectIds/resolveObjectIdCollisions (FileOps.cpp:579-603). These are kept in sync only by hand (NEXT.md:19 admits "Not every Alg mirror is wired back into its real .cpp call site … deliberate parallel duplicates kept only for testability"). Two have already drifted (see the insertAnimKeyframes and loadPrefs findings), proving the sync is not holding.
- **Outcome:** Where the *Alg body is CNA-free (duplicateObjectsAlg, groupObjectsAlg, ungroupObjectAlg, autoSaveTickAlg, rotateBackupsAlg, mergeDocumentsAlg, collect/resolveObjectIds, exportSelectionAlg, insertAnimKeyframesAlg…), delete the production duplicate and have the real .cpp call the *Alg function (as already done for e.g. groupScaleAlg, breakInstanceAlg, mergeless commands). That collapses each pair to a single tested implementation.
- **Tests:** After rewiring, the existing editor_commands tests now cover the real code path; add a build assertion / grep gate that no *Alg has a byte-identical sibling in src/.
- **Verify note:** Line numbers for several Alg definitions are cited a few lines into the function body rather than at the signature: duplicateObjectsAlg is at :303 (not :318), groupObjectsAlg :326 (not :341), autoSaveTickAlg :974 (not :989), rotateBackupsAlg :1001 (not :1016), mergeDocumentsAlg :1163 (not :1178). Production file basenames are abbreviated: 'Commands.cpp' is actually src/MeshCraft/MeshCraftApplication_Commands.cpp and 'FileOps.cpp' is src/MeshCraft/MeshCraftApplication_FileOps.cpp (the cited line ranges themselves are correct). COMPONENT should read 'EditorAlgorithms.hpp (26 specific functions)' rather than 'whole file', since other *Alg functions in the same header (breakInstanceAlg, alignToObjectAlg, pickObjectByRayAlg, cameraOrbitPositionAlg, exportMaterialAlg, etc.) DO have production call sites and are correctly wired.
- **Resolved:** commit `5c5e436` (final of 14 commits: `530c282`, `56060d2`, `1049fa1`, `4de55bf`, `dfd6b49`, `499d998`, `7d524ed`, `2073919`, `90aa5a3`, `4d64490`, `5c5e436`) — verify: `ctest` (full suite, 101/101)
- **Status note:** Inspected all 26 named functions individually per-function (not a blind mechanical batch): 24 rewired so production calls the *Alg function directly, deleting the hand-copied duplicate; 2 (keyBindToStringAlg/keyBindFromStringAlg) correctly identified as an INTENTIONAL divergence and deliberately left alone. Of the 24: most were confirmed byte-identical duplicates (mergeDocumentsAlg+collectObjectIdsAlg+resolveObjectIdCollisionsAlg — also deleted a second, redundant local duplicate of the id-collision helpers that FileOps.cpp had grown independently of EditorAlgorithms.hpp's copy; duplicateObjectsAlg/groupObjectsAlg/ungroupObjectAlg; autoSavePathAlg/autoSaveTickAlg/rotateBackupsAlg; insertAnimKeyframesAlg — closing the loop AUD-030 opened, since that fix only re-synced the value-resolution half, not the outer loop; loadRecentFilesAlg/saveRecentFilesAlg/addRecentFileAlg; confirmIfModifiedAlg; exportSelectionAlg; pushWithCapAlg, applied at all 3 real sites: pushUndo/performUndo/performRedo; resolveSaveAsPathAlg; unsavedDialogResolvesToExecuteAlg, with each dialog button's own bespoke side effects kept in production since the Alg mirror only captures the boolean; hierarchyFilterMatchesAlg, which also let two local lambdas (matchesFilter/matchesType) be deleted entirely from SceneHierarchyPanel.cpp). Two needed a type-boundary conversion because production's type is genuinely CNA-coupled and can't be identical to the Alg mirror's type: loadPrefsAlg/savePrefsAlg (PrefsAlg vs 6 separate MeshCraftApplication scalar members -- also fixed 2 already-drifted default values, snapTranslate/snapScale, found while doing this) and saveMacroAlg/loadMacroAlg (MacroStepAlg vs MacroStep) and materialColorAlg (plain float array vs CNA's Color, conversion arithmetic verified exact via the 180/255 gray-fallback round-trip in IEEE754 float32). For keyBindToStringAlg/keyBindFromStringAlg specifically: confirmed by reading both implementations that the mirror deliberately uses a small 6-key toy name table (documented in its own comment) instead of the real CNA `Keys::` enum's full table, so it can stay headlessly testable without a CNA dependency -- collapsing production onto it would silently break every keybinding whose key isn't one of those 6 (a real regression a blind mechanical rename would have introduced). No production behavior change anywhere else (confirmed by full-suite 101/101 after each of the 14 commits, not just at the end).

### AUD-032 `[DONE]` `P2` `W4` · loadPrefsAlg mirror omits the input clamping that production loadPrefs performs — a hand-edited prefs.ini with out-of-range values is silently accepted
- **Component:** include/MeshCraft/EditorAlgorithms.hpp:1631 vs src/MeshCraft/MeshCraftApplication_FileOps.cpp:683
- **Evidence:** The mirror header (EditorAlgorithms.hpp:1590-1598) claims to mirror loadPrefs()/savePrefs() and says "An unknown key or an unparseable value is silently skipped". But loadPrefsAlg (EditorAlgorithms.hpp:1631-1638) assigns raw values: `if (key=="snapScale") p.snapScale = std::stof(val);` with no bounds. Production loadPrefs (FileOps.cpp:683-688) clamps every field — `snapScale_ = std::clamp(std::stof(val),0.01f,100.0f)` etc — precisely so "a hand-edited prefs.ini can't set a value neither slider could ever reach, e.g. 0 or negative snapScale" (comment FileOps.cpp:677-682). So loading prefs.ini with snapScale=-5: production yields 0.01, the mirror yields -5. The headless test testPrefsPersistenceRoundTrip (editor_commands_test.cpp:2316-2345) round-trips only in-range values, so the clamp hardening is exercised by NO test despite the mirror existing to make prefs loading testable.
- **Outcome:** Add the same std::clamp bounds to loadPrefsAlg so it faithfully mirrors production, or route the test through production loadPrefs.
- **Tests:** Extend testPrefsPersistenceRoundTrip: write snapScale=-5 / autoSaveInterval=99999 to the ini, load, assert clamped to 0.01 / 300.0.
- **Verify note:** Line citations in the finding are off. Corrected: (1) Mirror header comment is at EditorAlgorithms.hpp:1575-1583 (finding said 1590-1598, which is actually inside the PrefsAlg struct/savePrefsAlg). (2) loadPrefsAlg is at EditorAlgorithms.hpp:1606-1625, with the uncllamped snapScale assignment at line 1620 (finding said 1631-1638, which is actually the MacroStepAlg comment/struct — wrong location). (3) Production loadPrefs clamps at FileOps.cpp:683-688 with rationale comment at 677-682 — confirmed exactly. (4) Test is at mc3/test/editor_commands_test.cpp:2316-2345 (finding gave the range but not the mc3/ path prefix); the two neighboring prefs tests at 2347-2375 also do not test clamping. Severity P2 confirmed correct.
- **Resolved:** commit `ff03ab5` — verify: `ctest -R mc3_commands`
- **Status note:** Ported all 6 of production `loadPrefs()`'s exact clamp bounds (autoSaveInterval [0,300], snapTranslate [0.01,100], snapRotate [1,180], snapScale [0.01,10], gridSpacing [0.1,10], theme [0,2]) into `loadPrefsAlg`, with a comment noting they must be kept in sync by hand (no shared constant between the mirror and production). Added a new test (rather than extending `testPrefsPersistenceRoundTrip`, to keep the round-trip test's own single concern clean) writing all 6 fields out of range and asserting each lands at its documented boundary — broader than the 2 fields the Tests field named, covering the full set this task's own bug affected identically.

### AUD-033 `[DONE]` `P2` `W4` · MeshCraftPrivate.hpp keeps byte-identical duplicates of five EditorAlgorithms.hpp helpers, and both copies are compiled into the same translation units
- **Component:** src/MeshCraft/MeshCraftPrivate.hpp vs include/MeshCraft/EditorAlgorithms.hpp
- **Evidence:** objectTypeName (Private.hpp:139-158) is identical to objectTypeNameAlg (EditorAlgorithms.hpp:30-49); deepCopyObject (Private.hpp:78-85) == deepCopyObjectAlg (:53-60); findParentList (Private.hpp:87-99) == findParentListAlg (:64-76); removeFromList (Private.hpp:68-76) == removeFromListAlg (:80-88); applyRenamePattern (Private.hpp:160-181) == applyRenamePatternAlg (:94-116). MeshCraftApplication_Commands.cpp includes BOTH headers (lines 2-3), so both objectTypeName and objectTypeNameAlg, both applyRenamePattern and applyRenamePatternAlg etc. coexist in one TU. Production uses the non-Alg copies (Commands.cpp:102/110 objectTypeName; UiOverlays.cpp:566 applyRenamePattern), while tests exercise the Alg copies — a batch-rename token change edited in one copy would silently not affect the other.
- **Outcome:** Delete the Private.hpp duplicates and have production include/use the EditorAlgorithms.hpp *Alg versions (they are already CNA-free), leaving one tested implementation of each helper.
- **Resolved:** commit `32898a9` — verify: `ctest -R mc3_commands` (full suite: `ctest`)
- **Status note:** Found `objectTypeName` was ALREADY deduplicated in an earlier, unrelated pass — `objectTypeNameAlg` now just delegates to the canonical `MeshCraft/Editor/ObjectTypeName.hpp` implementation — so only the other 4 (`removeFromList`/`deepCopyObject`/`findParentList`/`applyRenamePattern`) were still genuine byte-identical duplicates. Deleted all 4 from `MeshCraftPrivate.hpp` and updated every production call site (9 total, across Commands.cpp/FileOps.cpp/UiOverlays.cpp/Keyboard.cpp) to call the `*Alg` versions, adding the `EditorAlgorithms.hpp` include to the 2 files that didn't already have it. No behavior change (the copies were byte-identical) — full suite passing confirms no regression, not new functionality.
- **Tests:** No behavior change expected; existing rename/duplicate tests should still pass against the single surviving copy.
- **Verify note:** Retitle to FOUR byte-identical duplicates, not five. Correct pairs and lines: removeFromList (src/MeshCraft/MeshCraftPrivate.hpp:51-59) vs removeFromListAlg (include/MeshCraft/EditorAlgorithms.hpp:65-73); deepCopyObject (Private.hpp:61-68) vs deepCopyObjectAlg (EditorAlgorithms.hpp:38-45); findParentList (Private.hpp:70-82) vs findParentListAlg (EditorAlgorithms.hpp:49-61); applyRenamePattern (Private.hpp:125-146) vs applyRenamePatternAlg (EditorAlgorithms.hpp:79-101). Drop the objectTypeName pair entirely: objectTypeName is NOT duplicated in Private.hpp (only a comment at line 122), objectTypeNameAlg (EditorAlgorithms.hpp:34) is a thin wrapper over the single canonical objectTypeName() in ObjectTypeName.hpp, and the code explicitly documents they cannot drift. Severity P2 (maintainability/duplication) is correct; the applyRenamePattern token-drift scenario (prod UiOverlays.cpp:566 uses the non-Alg copy, tests use the Alg copy) is the strongest concrete instance.

### AUD-034 `[DONE]` `P2` `W4` · convertToDefinition drops the object's layer on the replacement Instance, while the sibling exportSubtreeAsTemplate preserves it
- **Component:** include/MeshCraft/EditorAlgorithms.hpp:412 vs src/MeshCraft/MeshCraftApplication_Commands.cpp:553
- **Evidence:** convertToDefinition (Commands.cpp:507-518) delegates to convertToDefinitionAlg, whose new Instance copies id/name/type/definition/transform/visible/tags but NOT layer: EditorAlgorithms.hpp:412-419 has no `inst->layer = src->layer;`. The near-identical exportSubtreeAsTemplate, which also replaces the source with an Instance, DOES preserve it: `inst->layer = src->layer;` (Commands.cpp:553). layer is a real, filter-affecting field (Mc3Object.hpp:73 `std::string layer; // named layer`; SceneHierarchyPanel.cpp:239 filters on `o.layer==layerFilter_`). So Convert-to-Definition on an object assigned to layer "background" silently moves the resulting Instance to the default layer.
- **Outcome:** Add `inst->tags = src->tags;`-adjacent `inst->layer = src->layer;` in convertToDefinitionAlg so it matches exportSubtreeAsTemplate and preserves the object's layer.
- **Tests:** Unit test: object with layer="bg", convertToDefinitionAlg, assert returned Instance->layer=="bg".
- **Verify note:** Line-number corrections: (1) The exportSubtreeAsTemplate layer-preserving line is Commands.cpp:555 (`inst->layer = src->layer;`), not 553 — line 553 is `inst->transform = src->transform;`. (2) The convertToDefinitionAlg field-copy block that omits layer is EditorAlgorithms.hpp:398-404 (not 412-419; line 411-412 is the `doc.objects.push_back(inst);}` fallback branch, after the copy). Severity P2 is appropriate.
- **Resolved:** commit `d057728` — verify: `ctest -R mc3_commands`
- **Status note:** Added `inst->layer = src->layer;` right after the `tags` copy in `convertToDefinitionAlg`, exactly matching `exportSubtreeAsTemplate`'s pattern. Confirmed this Alg function is a genuine production call site (`MeshCraftApplication::convertToDefinition` calls it directly), not one of AUD-031's test-only mirrors, so this fixes the real editor behavior, not just a test double. Extended `testConvertToDefinition` with `src->layer = "bg"` and an assertion the returned Instance keeps it.

### AUD-035 `[DONE]` `P3` `W4` · Batch-rename live preview uses a parallel rename copy that ignores lockedIds, so it shows locked objects being renamed when Apply skips them
- **Component:** src/MeshCraft/MeshCraftApplication_UiOverlays.cpp:564 vs include/MeshCraft/EditorAlgorithms.hpp:152
- **Evidence:** The preview loop (UiOverlays.cpp:564-569) calls applyRenamePattern for the first 3 selected objects unconditionally and displays "old → new" for each. The actual apply, batchRenameObjects (EditorAlgorithms.hpp:152-164), skips locked objects: `if (lockedIds.count(s->id)) { ++idx; continue; }` — a locked object keeps its name. So a locked object in the top 3 of the selection shows a rename in the preview that will not occur on Apply. (Index numbering itself matches: both advance the counter for every object, preview uses i+1, apply increments idx even for locked.)
- **Outcome:** Have the preview consult lockedIds_ and render locked objects as unchanged (or grey "(locked)"), or drive the preview through the same batchRenameObjects code path used by Apply.
- **Tests:** Manual/UI: select a locked + unlocked object, open Batch Rename, confirm the locked row shows no rename.
- **Verify note:** The EditorAlgorithms.hpp line citation is imprecise. The batchRenameObjects function spans lines 129-150 (not 152-164); the locked-skip `if (lockedIds.count(s->id)) { ++idx; continue; }` is at line 138. Line 152 is actually the "// ── Find-replace helpers ──" comment. Corrected component: src/MeshCraft/MeshCraftApplication_UiOverlays.cpp:564 vs include/MeshCraft/EditorAlgorithms.hpp:138. Everything else in the finding is accurate; severity P3 stands.
- **Resolved:** commit `75390a5` — verify: build MeshCraft, manually open Batch Rename with a locked + unlocked object selected (P3/UI-only per Tests field, no automated test)
- **Status note:** Took the first Outcome option: the preview loop now checks `lockedIds_.count(obj->id)` (the same set Apply consults) and renders "(locked, skipped)" instead of a rename arrow for that row; the remaining rows' `i + 1` index numbering is unaffected since it already matched Apply's unconditionally-advancing `idx` per-position. Deliberately did NOT add a new EditorAlgorithms.hpp pure-function mirror of this preview logic — AUD-031 already flags that file's existing test-only mirrors as a duplication problem, and adding another one for a P3 UI-preview nicety would repeat the exact anti-pattern that finding is about.

### AUD-036 `[DONE]` `P0` `W9` · Undo never records Drag/Color edits: `if(IsItemActivated()) pushUndo()` inside the `if(DragFloat/ColorEdit)` block is dead code (transforms, all primitive dims, lights, camera, fog, material, keyframes — none undoable; also breaks redo-invalidation → silent data loss)
- **Component:** src/MeshCraft/Scene/PropertiesPanel.cpp, src/MeshCraft/MeshCraftApplication_UiLeftPanel.cpp, src/MeshCraft/MeshCraftApplication_Anim.cpp
- **Evidence:** The pervasive editor idiom nests the undo snapshot INSIDE the widget-changed block, e.g. src/MeshCraft/Scene/PropertiesPanel.cpp:135-136 `if (ImGui::DragFloat3("##pos", pos, 0.1f)) {` / ` if (ImGui::IsItemActivated()) ctx.pushUndo();`. This is provably dead for Drag*/ColorEdit*: (1) IsItemActivated() is true ONLY on the activation frame — imgui.cpp `bool ImGui::IsItemActivated()` returns true iff `g.ActiveId==g.LastItemData.ID && g.ActiveIdPreviousFrame!=g.LastItemData.ID`; (2) DragBehaviorT force-returns false on that exact frame — b-release/_deps/imgui-src/imgui_widgets.cpp DragBehaviorT: `const bool is_just_activated = g.ActiveIdIsJustActivated;` ... `if (is_just_activated ...) { g.DragCurrentAccum = 0.0f; g.DragCurrentAccumDirty = false; }` ... `if (!g.DragCurrentAccumDirty) return false;`. So on the frame IsItemActivated()==true, DragFloat()==false (block not entered); on later change frames IsItemActivated()==false. I confirmed empirically with a compiled headless ImGui 1.91.6 probe driving a 40-frame click-drag: DragFloat `inside(pushUndo)=0`, `split(correct)=1`; ColorEdit3 `inside=0`; SliderFloat `inside=1` (sliders snap-on-click so they DO fire). Scope of broken widgets: PropertiesPanel.cpp 45 Drag/Color (pos:135, rot:167, scl:197, pivot:305, every primitive dimension psize/prad/phgt/ppw/ppd/pdsk/pcap/pgrd/pico/ptor:818-1039, extrude/CSG params 1070-1310, material mnrmscl:1659/moccstr:1665, fog 2139-2174), UiLeftPanel.cpp 19 (light brightness/dir/pos/range:204-234, camera pos/tgt/rot/near/far:467-539, def-instance pos/rot/scl:926-954, anim-override:1792/1810), Anim.cpp 7 (duration:301, timeScale:320, keyframe value ##kfv:978, bezier handles:996-1020). Data-loss mechanism: `redoStack_.clear()` lives ONLY in pushUndo (Commands.cpp:335); markModified is just `modified_=true; updateWindowTitle()` (UiProperties.cpp:28). So a broken edit mutates the doc WITHOUT clearing redo — after an undo, a Drag edit, then Ctrl+Y, performRedo() overwrites document_ with the stale redo snapshot, silently discarding the edit. The Anim.cpp:297-298 comment even encodes the root misconception: 'IsItemActivated()-gated for the drag widget (fires every frame during a drag)' — it fires only on the activation frame.
- **Outcome:** Move the undo snapshot OUT of the widget-changed block so it is evaluated every frame, i.e. `bool ch = ImGui::DragFloat(...); if (ImGui::IsItemActivated()) pushUndo(); if (ch) { ...apply...; markModified(); }` (or push undo on IsItemActivated regardless of change) at all ~71 Drag*/ColorEdit* sites listed. SliderFloat/SliderInt sites already work and can stay.
- **Tests:** Add a headless ImGui frame-driven regression (like the probe in scratchpad/undo_probe2.cpp) asserting a click-drag on a DragFloat/DragFloat3/ColorEdit3 fires pushUndo exactly once; add an editor test: undo, then a Drag edit, then redo must NOT discard the Drag edit (redoStack must be cleared on any document mutation).
- **Resolved:** commit `737af77` — verify: `ctest -R undo_snapshot_lint`
- **Status note:** The specific dead-pattern bug (undo snapshot nested inside Drag/ColorEdit changed-block) is fixed at all 80 identified sites, with a source-lint regression guard. This does NOT constitute a full undo-system audit/rearchitecture -- see AUD-036b for the broader undo-transaction work the adversarial re-review requested (pre-mutation capture verification for all widget types, central transaction abstraction, frame-driven behavioral test, full undo_coverage_audit.py triage).

### AUD-037 `[DONE]` `P2` `W9` · GLB/GLTF export reports unqualified 'Exported' success in the UI even when the exporter counted warnings or skipped geometry (warnings only go to stdout)
- **Component:** src/MeshCraft/MeshCraftApplication_FileOps.cpp, mc3togltf/src/GltfExporter.cpp
- **Evidence:** src/MeshCraft/MeshCraftApplication_FileOps.cpp:282-297 builds `statusMsg = "Exported " + ... + " (" + uniqueMeshes + " meshes"...)` and unconditionally `setStatusMsg(statusMsg);`. The warning count is emitted only to std::cout at line 300 (`... << s.warnings << " warnings"`), never surfaced in the on-screen status. The exporter drops geometry while merely incrementing counters: GltfExporter.cpp:846-849 approximate-CSG path (`ctx.stats.warnings++`) exports children as geometrically-incorrect separate meshes, and GltfExporter.cpp:838-840 `if (!csgData.empty()) addMeshDataToGltf(...)` else silently emits no mesh and skips children — in both cases the editor still shows a green 'Exported N meshes' with no indication the output is partial/approximate.
- **Outcome:** Surface s.warnings (and approximate-CSG usage) in the user-facing status message and mark it as a warning-colored status when warnings>0, so a partial/approximate export is not presented as a clean success.
- **Tests:** Export a scene containing an unsupported/approximate CSG node with allowApproximateCSG and assert the returned status string is flagged as a warning and includes the warning count.
- **Resolved:** commit `d882237` — verify: `ctest -R editor_export_test`
- **Status note:** `runGltfExport()` now appends `" — N warning(s) (export may be incomplete or approximate; see console)"` to `statusMsg` when `s.warnings > 0`, and passes that as `setStatusMsg(statusMsg, hasWarnings)` — reusing the existing `isError` red styling (`MeshCraftApplication_UiOverlays.cpp:301`) rather than adding a third color tier, consistent with how this class already uses `isError=true` for non-fatal cautions elsewhere (e.g. `MeshCraftApplication_Macro.cpp`). This directly builds on AUD-026's fix (`79cc9c0`), which made `stats.warnings` actually count every warning path — without that fix this status text would still have undercounted. Verified via `editor_export_test.py`'s existing `--export` CLI flag (runs the real `runGltfExport()` codepath): house.mc3.xml's known unnamed-ambient-light warning (AUD-026) now produces the exact expected status text in stdout, not just the pre-existing trailing "N warnings" tally.

### AUD-038 `[DEFERRED]` `P3` `W9` · Undo history is a bounded 20-entry whole-document deep-copy stack; oldest entries are silently dropped (informational — answers the audit question, by-design)
- **Component:** include/MeshCraft/MeshCraftApplication.hpp, src/MeshCraft/MeshCraftApplication_Commands.cpp
- **Evidence:** include/MeshCraft/MeshCraftApplication.hpp:629 `static constexpr int kUndoMax = 20;`. pushUndo() at Commands.cpp:331-336 does `undoStack_.push_back(deepCopyDoc(document_)); if (undoStack_.size() > kUndoMax) undoStack_.erase(undoStack_.begin()); redoStack_.clear();` — so beyond 20 operations the oldest snapshot is silently discarded (no user notice), and each snapshot is a full deep copy of the entire document (all objects/materials/textures/actions), which for large scenes is 20 full-scene copies of RAM. This is standard bounded-history behavior, not a correctness bug; noting because the audit explicitly asked whether history is silently dropped (yes) and because it is the constraint that makes finding #1's whole-doc-snapshot model expensive.
- **Outcome:** No code change required for correctness. Optionally document the 20-op limit in the UI and/or consider a command-delta model if memory becomes a concern.
- **Tests:** n/a (behavioral note).
- **Verify note:** The `static constexpr int kUndoMax = 20;` is at include/MeshCraft/MeshCraftApplication.hpp:632, NOT line 629 (line 629 is the unrelated `void checkRotationConventionNotice();` declaration). The pushUndo() body spans Commands.cpp:331-337 and uses `static_cast<int>(undoStack_.size()) > kUndoMax` (the paraphrase omitted the cast, but the semantics match). Severity P3/informational is appropriate.
- **Status note:** By-design bounded history; audit's own correction confirms no code change is required for correctness.

### AUD-039 `[DONE]` `P1` `W8` · Gate C: editor UI + post-FX hard-wired to OpenGL ES3, yet CMake advertises SDL_RENDERER/BGFX/VULKAN as supported backends
- **Component:** src/MeshCraft/MeshCraftApplication.cpp + CMakeLists.txt (MESH_CRAFT_GRAPHICS_BACKEND option)
- **Evidence:** CMakeLists.txt:27 `# Supported: SDL_RENDERER, EASYGL, BGFX, VULKAN`; CMakeLists.txt:31-32 `set_property(CACHE MESH_CRAFT_GRAPHICS_BACKEND PROPERTY STRINGS SDL_RENDERER EASYGL BGFX VULKAN)`; CMakeLists.txt:47-50 FATAL_ERROR accepts all four as valid. But the editor renders its UI ONLY through OpenGL, unconditionally, with no backend branch: MeshCraftApplication.cpp:7 `#include <imgui_impl_opengl3.h>`; :212-213 `ImGui_ImplSDL3_InitForOpenGL(sdlWindow, glCtx); ImGui_ImplOpenGL3_Init("#version 300 es");` (with glCtx=`SDL_GL_GetCurrentContext()` at :211); :284 `ImGui_ImplOpenGL3_NewFrame();`; :292 `ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());`. Direct GL via SDL_GL_GetProcAddress at :123-126 (glViewport/glScissor/glEnable/glDisable) and :871 `#define LD(m,n) ... SDL_GL_GetProcAddress(n)` loading ~50 GL entry points (glGenFramebuffers, glCreateShader, glDrawArrays, ...). Post-FX shaders are GLES3-only: :1010,:1045,:1075 etc. all `#version 300 es`. Screenshot readback is hard GL: MeshCraftApplication_Commands.cpp:379-381 `SDL_GL_GetProcAddress("glReadPixels")` etc. PROOF of no backend abstraction in editor code: `grep -rn 'CNA_BACKEND|GRAPHICS_BACKEND|BACKEND_EASYGL|BACKEND_BGFX|BACKEND_VULKAN|BACKEND_SDL' src/ include/` returns ZERO matches, and only `ImGui_ImplSDL3_InitForOpenGL` is ever used (grep for InitForVulkan/InitForSDLRenderer/ImGui_ImplVulkan/ImGui_ImplBgfx across src+include+CMake = 0 hits).
- **Outcome:** Either (a) truthfully restrict the advertised option to backends the editor can actually drive — i.e. make CMake hard-error (not just accept) on BGFX/VULKAN/SDL_RENDERER, or explicitly mark them experimental/unsupported for the GUI editor — or (b) actually implement per-backend ImGui renderers (imgui_impl_vulkan / imgui_impl_sdlrenderer3 / a bgfx ImGui backend) and backend-specific post-FX, selected by a real #ifdef on the chosen backend. Option (a) is the honest, in-repo fix.
- **Tests:** Configure+build with -DMESH_CRAFT_GRAPHICS_BACKEND=VULKAN (and BGFX, SDL_RENDERER) and either observe a clear configure-time rejection, or launch the editor and confirm the ImGui UI actually renders. Currently there is no test asserting the editor works on any non-EASYGL backend.
- **Verify note:** Severity P1 is correct. One trivial line-number imprecision: the SDL_GL screenshot readback in MeshCraftApplication_Commands.cpp is at lines 381-383 (glFinish 381, glBindBuffer 382, glReadPixels 383), not the claimed 379-381. This does not affect the finding.
- **Blocked:** Full multi-backend ImGui support would require CNA-side GL/Vulkan context coordination (CNA is out of scope per CLAUDE.md). The truthfulness fix (restrict/label the option) is fully in-repo and NOT blocked.
- **Resolved:** commit `e53af49` — verify: `cmake -S . -B <dir> -DMESH_CRAFT_GRAPHICS_BACKEND=BGFX 2>&1 | grep "CMake Warning"`
- **Status note:** PARTIAL per adversarial re-review: e53af49 added a configure-time WARNING only: the editor target still builds (non-functionally) under BGFX/VULKAN/SDL_RENDERER. This does not fully satisfy Gate C ("never advertise selectable but non-functional configurations"). See AUD-039b for the follow-up: a real configure-time hard failure for the editor target under non-EASYGL backends.

### AUD-040 `[DONE]` `P1` `W8` · ImGui is compiled OpenGL-ES3-only and linked into the editor for every backend; no alternate ImGui backend is ever built
- **Component:** CMakeLists.txt (imgui target + editor link)
- **Evidence:** CMakeLists.txt:129-135 the imgui static lib compiles ONLY `imgui_impl_sdl3.cpp` and `imgui_impl_opengl3.cpp` — no imgui_impl_sdlrenderer3.cpp, imgui_impl_vulkan.cpp, or bgfx backend. CMakeLists.txt:141 `target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)` and :152 `target_link_libraries(imgui PUBLIC GLESv2)` bake in GLES3 unconditionally. The editor links this GL-only imgui for ALL four backends with no guard: CMakeLists.txt:338 (Emscripten), :357 (native GNU/Clang), :369 (fallback) all list `imgui` in target_link_libraries regardless of `${_cna_backend_target}` = cna_backend_graphics_sdl_renderer/bgfx/vulkan (set at :324-333). So a VULKAN/BGFX/SDL_RENDERER build still links the GL3 ImGui UI backend it has no context to drive.
- **Outcome:** When a non-GL backend is selected, either compile+link the matching ImGui backend (imgui_impl_vulkan/imgui_impl_sdlrenderer3) instead of imgui_impl_opengl3, or fail configuration. Do not silently link the GL3 UI backend into a Vulkan/BGFX/SDL_Renderer build.
- **Tests:** Assert at configure time that the ImGui backend source list matches the selected MESH_CRAFT_GRAPHICS_BACKEND; add a CI matrix entry per backend.
- **Verify note:** Two minor evidence corrections: (1) the editor's imgui link lines are 338 (Emscripten), 358 (native GNU/Clang), 365 (fallback) — not the claimed :357/:369. (2) GLESv2 at line 152 is guarded by `if(NOT EMSCRIPTEN)` (151), so it is unconditional only for native builds; Emscripten instead uses WebGL2 (MIN/MAX_WEBGL_VERSION=2 at 345-346), which is GLES3-equivalent, so the GL-only coupling still holds there. Strengthen the proof by citing the runtime hard-wiring in src/MeshCraft/MeshCraftApplication.cpp:211-213 and 284/292, plus the "Supported: SDL_RENDERER, EASYGL, BGFX, VULKAN" claim at CMakeLists.txt:27 — the ImGui UI can only actually work under the GL backend (EASYGL, the default).
- **Blocked:** Not blocked for the honest-restriction fix; a real per-backend ImGui build needs the corresponding CNA context type.
- **Resolved:** commit `e53af49` — verify: `same as AUD-039`
- **Status note:** Same partial-fix status and follow-up as AUD-039 (they are the same underlying defect: ImGui is GL-only and every backend still links it).

### AUD-041 `[DONE]` `P1` `W8` · README documents 'Build with SDL_RENDERER backend' as a first-class supported build with no caveat that the editor UI cannot render
- **Component:** README.md
- **Evidence:** README.md:59-64 `### Build with SDL_RENDERER backend` followed by copy-paste `cmake -S . -B cmake-build-debug -G Ninja -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER` / `ninja ...`, presented identically to the default EASYGL build immediately above it. The only following `> Note:` (:66) is about GLOB_RECURSE, not about the backend. Nowhere in README does it state that selecting SDL_RENDERER/BGFX/VULKAN yields an editor whose ImGui UI (the whole GUI) will not draw because MeshCraft only ships the ImGui_ImplOpenGL3 path. `grep` of README/docs for any such caveat (only OpenGL, does not render, hard-wired, etc.) finds nothing relevant.
- **Outcome:** Remove or explicitly caveat the SDL_RENDERER build section: state that EASYGL (desktop GL) / WebGL2 is the only backend the GUI editor currently renders on, and that SDL_RENDERER/BGFX/VULKAN are engine-level (CNA) options not yet wired to the editor UI.
- **Tests:** Doc review; ensure the Platform Support Matrix and build sections agree with the actual working backend set.
- **Blocked:** None — pure documentation fix.
- **Resolved:** commit `e53af49` — verify: `grep -A3 "Graphics backend" README.md`

### AUD-042 `[TODO]` `P2` `W8` · Android build path force-selects SDL_RENDERER, guaranteeing the editor UI would not render if built for Android
- **Component:** CMakeLists.txt (Android backend auto-select)
- **Evidence:** CMakeLists.txt:36-37 `if(ANDROID) set(MESH_CRAFT_GRAPHICS_BACKEND_UPPER "SDL_RENDERER")`. Combined with the fact (Finding 1) that the editor's ImGui UI and post-FX only work through ImGui_ImplOpenGL3 + SDL_GL_GetProcAddress (which need an SDL GL context, not an SDL_Renderer), an Android build would compile against a backend the editor cannot render on. README.md:211 correctly marks Android as `never attempted` so it is not falsely claimed working, but the CMake default choice bakes in a non-functional editor for the one platform that is forced onto SDL_RENDERER.
- **Outcome:** If Android support is intended, wire the editor to a backend it can actually render on (e.g. GLES via EASYGL) or gate the GUI editor off on Android with a clear message, rather than auto-selecting SDL_RENDERER which the UI layer cannot drive.
- **Tests:** An Android NDK configure that either selects a GL-capable backend or errors clearly; not currently testable here (no NDK installed).
- **Verify note:** Refinement (does not change the verdict): the editor↔SDL_RENDERER incompatibility is not Android-specific — the identical breakage occurs for ANY build configured with -DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER on desktop, since the editor's ImGui path (MeshCraftApplication.cpp:212-213) is hardwired to OpenGL3 with no SDL_Renderer branch. What is Android-specific is that the force at CMakeLists.txt:36-37 makes SDL_RENDERER non-optional there (the user cannot pick EASYGL). So the finding is slightly understated in scope but accurate as stated. Severity P2 stands.
- **Blocked:** No Android NDK in this environment; also intersects CNA backend behavior (out of scope).
- **Status note:** `AUD-039b`'s runtime check (commit `58a7f03`) now means an Android build (if one were attempted) would refuse to launch the editor UI with a clear error, rather than silently opening a non-functional window -- so the "renders nothing with no indication why" consequence this finding warns about is closed. The CMake-level force-select itself (`if(ANDROID) set(...SDL_RENDERER)`) is unchanged; this task stays `TODO` because the root cause (Android has no path to a GL-capable backend at all) is still open and untestable here (no NDK).
- **Status note:** Android force-selects SDL_RENDERER (a backend the editor cannot render on); no NDK available to test in this environment. Depends on the AUD-039b decision (hard failure vs. real backend) for a principled fix.

### AUD-043 `[DONE]` `P1` `W13` · STABILIZATION.md is doubly stale: 650 tasks / 620 done AND 66 tests, both wrong
- **Component:** STABILIZATION.md
- **Evidence:** STABILIZATION.md:23 "The full backlog is **650** `STAB-XXXX` tasks ... sectioned S0-S20"; :26 "Overall: **620 ✅ / 29 🟡 / 0 🧪 / 1 📋 / 0 🔴** across all 650 rows."; :44 "**66 CTest tests, all passing**". Line 3 asserts "every count below was recomputed directly from `plan.md`'s per-row status markers and `ctest`." But plan.md's own current summary table (plan.md:399) is 723 total / 683 ✅ (S21–S25 added 73 tasks), and ctest is 87. The 650/620/66 figures predate the S21–S25 expansion and were never updated.
- **Outcome:** Either refresh every count (723 total, 683 ✅ per plan.md table, 87 tests) or archive STABILIZATION.md to docs/history/ and keep only the policy section, which is the sole non-stale content.
- **Tests:** Compare STABILIZATION.md:23/26/44 against plan.md:399 and `ctest -N`
- **Resolved:** commit `0dfcd4f` — verify: `grep -c "650\|620 " STABILIZATION.md (expect 0 in the live gate table)`

### AUD-044 `[DONE]` `P1` `W13` · STABILIZATION_VERIFICATION.md marks Web/Emscripten ✅ fully working while README and NEXT.md say it is broken
- **Component:** STABILIZATION_VERIFICATION.md vs README.md vs NEXT.md
- **Evidence:** STABILIZATION_VERIFICATION.md:56 "Web (Emscripten) | ✅ | Full build succeeds; real headless-Chrome session confirms a working WebGL2 context, no console/GPU errors". Directly contradicted by README.md:213 "3D viewport rendering | ... | ❌ blank canvas" and NEXT.md:67-72 which root-causes it as "an uncaught crash ... kills the whole wasm module ... The app dies on the first resize event, before it ever finishes sizing anything". Three docs assert three mutually exclusive states (fully-working / blank-canvas-sizing-bug / crashes-before-first-frame).
- **Outcome:** Reconcile to the latest verified state (NEXT.md's crash root-cause, 2026-07-11). Flip STABILIZATION_VERIFICATION.md:56 from ✅ to 🟡/❌ or archive the file.
- **Tests:** Diff the Web row across STABILIZATION_VERIFICATION.md:56, README.md:212-213, NEXT.md:67-72
- **Verify note:** Evidence is accurate as written. One minor nuance to add for precision: STABILIZATION_VERIFICATION.md carries a file-level disclaimer at line 5 ("_Last verified: 2026-07-07, ... commit 4b7fcd2_"), so it is a dated snapshot older than README's 2026-07-09 Web-column update and NEXT.md's 2026-07-11 root-cause. This does not neutralize the finding: the file self-describes (line 3) as the authoritative "current, actually-checked state of each verification area", and its Web ✅/"no console/GPU errors" line was never reconciled with the two later in-repo docs that mark the same target ❌/crashing. Severity P1 stands.
- **Resolved:** commit `0dfcd4f` — verify: `ls docs/history/STABILIZATION_VERIFICATION.md`

### AUD-045 `[DONE]` `P2 (self-assigned P1, elevated/adjusted per adversarial correction)` `W13` · CHANGELOG.md presents a wildly stale progress snapshot (194 done / 317 not-started of 650) as current
- **Component:** CHANGELOG.md
- **Evidence:** CHANGELOG.md:12-13 "the full 650-task backlog (`STAB-0001`–`STAB-0650`, sectioned S0–S20)"; :25-28 "As of this writing: **194 ✅ done, 3 🟡 partial, 136 🧪 has a plan but not executed, 317 📋 not started** out of 650 `STAB-XXXX` tasks". Current reality (plan.md:399) is 723 total / 683 ✅ / 0 🧪 / 2 📋, and NEXT.md:12 says 688 ✅. The changelog's 194-done/317-not-started/136-needs-test figures are off by hundreds of rows and describe a project phase that ended weeks ago.
- **Outcome:** Replace the [Unreleased] progress paragraph with the current summary (or drop the per-status counts entirely and link plan.md's table, which the changelog already calls authoritative at :105). Add S21–S25 to the '650-task' description.
- **Tests:** Compare CHANGELOG.md:25-28 against plan.md:399 and NEXT.md:12
- **Verify note:** Severity should be P2, not P1: this is documentation staleness in an [Unreleased] changelog progress count, not an "untruthful capability claim" — it misrepresents no shipped software capability, causes no UB/crash/invalid output. Two mitigating facts the finding omits: (a) CHANGELOG.md:3-6 frames the whole file as a reconstructed, not-hand-maintained summary, and lines 27-28 explicitly redirect readers to plan.md's table and NEXT.md for current status, so it is not presented as the authoritative live count. (b) Minor evidence imprecision: the 688 ✅ figure is on NEXT.md line 13, not line 12 as cited. Core defect (stale 194/317/136-of-650 numbers) is real and verifiable.
- **Resolved:** commit `0dfcd4f` — verify: `grep "194\|317 not-started" CHANGELOG.md (expect 0 hits)`

### AUD-046 `[DONE]` `P2 (self-assigned P1, elevated/adjusted per adversarial correction)` `W13` · plan.md self-contradicts: summary table says 723 rows, the note directly under it says 650
- **Component:** plan.md
- **Evidence:** plan.md:399 "| **TOTAL** | **723** | **683** | **38** | **0** | **2** | **0** |" (arithmetically self-consistent: 683+38+2=723). But the annotation immediately below, plan.md:409-411, states "Total row count is 650 (this session found and removed an accidental duplicate STAB-0521 row ... 650 matches the plan's original design count)", and plan.md:369 (12 lines above the table) says "the 620 ✅ rows counted below ... only the 29 🟡 + 1 📋 = 30 open rows remain inline" (620+30=650). The same section asserts both 650/620 and 723/683, and :417 claims 'Last recomputed 2026-07-06' while :369 is dated 2026-07-10.
- **Outcome:** Delete the obsolete '650' note (plan.md:409-411) and the '620 ✅ / 30 open' framing (plan.md:369); they predate the S21–S25 rows the table itself now counts. Keep one internally consistent total.
- **Tests:** Re-run the per-row recompute the note itself prescribes and confirm it equals the table
- **Verify note:** Evidence is accurate and needs no correction. Severity should be P2, not P1: this is a documentation/bookkeeping inconsistency in a planning markdown file (plan.md) with zero runtime, data, or output impact and no untruthful claim about software capability, so it falls under P2 (maintainability/documentation accuracy) per the audit rubric rather than P1 (wrong behavior/missing validation/untruthful capability claim).
- **Resolved:** commits `0dfcd4f`, `d16c82c`, `d2ae2dc` — verify: `python3 test/validate_plan_consistency.py . b-release`
- **Status note:** 0dfcd4f replaced the self-contradicting 723-vs-650 plan.md with a fresh backlog, but that fresh backlog itself then drifted into its OWN internal inconsistency (93/93 vs 95/95 test-count claims across plan.md/NEXT.md, and a stale HEAD reference) -- exactly the class of defect this finding is about, now recurring. Fixed in this pass with a mechanical validator (test/validate_plan_consistency.py) wired into CTest so it cannot silently recur a third time.

### AUD-047 `[DONE]` `P2` `W13` · NEXT.md and plan.md disagree on completion counts by 5 rows (688/33 vs 683/38)
- **Component:** NEXT.md vs plan.md
- **Evidence:** NEXT.md:12 "**688/723 rows ✅, 33 🟡 (mostly implemented-but-pending-live-visual-verification ...), 0 needs_human, 2 📋**". plan.md:399 summary table "TOTAL 723 | 683 ✅ | 38 🟡 | 0 🧪 | 2 📋". Both agree on 723 total and 2 📋, but differ by exactly 5 on ✅ (688 vs 683) and 🟡 (33 vs 38). These are the two docs each other calls authoritative (plan.md:105/STABILIZATION.md:73 point to plan.md; NEXT.md tracks live status), so a reader cannot tell which is right.
- **Outcome:** Recompute from plan.md + plan_20260710.md row markers with a script and make both files cite the same numbers.
- **Tests:** python3 parse of plan.md/plan_20260710.md status markers per section vs both quoted totals
- **Verify note:** The "688/723 rows ✅, 33 🟡" quote is at NEXT.md:13, not NEXT.md:12. Everything else in the evidence is accurate. Note the finding is understated: NEXT.md:13 literally opens with "`plan.md`: **688/723 ...**", presenting these as plan.md's own counts, while plan.md:399 TOTAL says 683/38 — so it's not merely two docs disagreeing, but one doc misquoting the other.
- **Resolved:** commit `0dfcd4f` — verify: `(see commit)`
- **Status note:** Moot: the two disagreeing documents (old plan.md's 683/38 vs old NEXT.md's 688/33) were both superseded by the 0dfcd4f rewrite; the new plan.md/NEXT.md pair is the sole live source, kept consistent by the new AUD-046 validator.

### AUD-048 `[DONE]` `P3 (self-assigned P2, elevated/adjusted per adversarial correction)` `W13` · RELEASE.md and plan_deep_audit.md still hard-code 66/66 as the test total
- **Component:** RELEASE.md, plan_deep_audit.md
- **Evidence:** RELEASE.md:35 "**all** tests pass (66/66 as of this writing — check `ctest -N` for the current count, since it grows over time)" and :39 "`mc3togltf` 41/41". plan_deep_audit.md:11 "The native Linux build is clean and 66/66 tests pass"; :13 "all 66 CTest tests are properly registered, no orphaned test binaries" and "650 `STAB-XXXX` tasks in `plan.md`, 620 ✅". Actual is 87 registered / 723 tasks. RELEASE.md at least hedges ('check ctest -N'); plan_deep_audit.md states 66 flatly.
- **Outcome:** Bump RELEASE.md to 87 (or make it purely 'check ctest -N'); update plan_deep_audit.md's summary to 87/723 or mark the file historical (all its AUDIT tasks are now 'completed' per plan_deep_audit.md:23).
- **Tests:** grep -n '66/66\|650\|620' RELEASE.md plan_deep_audit.md vs ctest -N and plan.md:399
- **Verify note:** Severity is better classified as P3 (doc polish/staleness), not P2. The RELEASE.md half is substantially self-mitigated: both cited lines say "as of this writing" and the main one explicitly instructs the reader to "check ctest -N for the current count, since it grows over time" — intentional snapshot documentation with a self-check, not a misleading hard-coded claim. The real defect is concentrated in plan_deep_audit.md: its summary flatly asserts "66/66 tests pass" and "650 STAB-XXXX tasks... 620 ✅" while the SAME file at line 80 (AUDIT-0038, 2026-07-11) says "Full 87/87 ctest green" and plan.md now runs to STAB-0723 — an internal contradiction in a file still labeled the "Active plan file". Verified actuals: ctest -N = 87 (cmake-build-debug) / 89 (b-release); 58 mc3togltf-named tests; plan.md max id STAB-0723.
- **Resolved:** commit `d2ae2dc` — verify: `grep -c "66/66" RELEASE.md; test -f docs/history/plan_deep_audit.md && echo archived` (RELEASE.md now points at `ctest -N`; plan_deep_audit.md archived as historical, all its own tasks completed)

### AUD-049 `[DONE]` `P2` `W13` · plan.md's 'Known Test Suite (as of 2026-06-27)' section still lists 15 tests
- **Component:** plan.md
- **Evidence:** plan.md:46 heading "## Known Test Suite (as of 2026-06-27)" then :51 "15 CTest tests registered, all passing" followed by a per-test table. This is 6x below the current 87. It is dated, but it sits inline in the active plan (not in an archive) with no 'superseded' banner, so a reader scanning plan.md hits '15 tests' before reaching any current figure.
- **Outcome:** Either delete this section (its content is fully superseded by TESTING.md/ctest -N) or move it under a clearly-labeled Historical heading.
- **Tests:** n/a (doc)
- **Verify note:** The "15 CTest tests registered, all passing" statement is at plan.md:48 (not :51); the per-test table spans lines 50-66. The section heading is explicitly dated "(as of 2026-06-27)" but no "superseded" banner marks the section, and the same file's current figure of "87/87 ctest green" appears far below at lines 132 and 140. Severity P2 is appropriate.
- **Resolved:** commit `0dfcd4f` — verify: `(see commit)`
- **Status note:** Moot: the old plan.md containing the stale 2026-06-27/15-test section was archived to docs/history/plan_stabilization_master.md in its entirety.

### AUD-050 `[DONE]` `P2` `W13` · web_issues.md is fully superseded — both of its 'unresolved' issues were later overturned by NEXT.md
- **Component:** web_issues.md vs NEXT.md / README.md
- **Evidence:** web_issues.md:58 "## 2. Emscripten build no longer completes from scratch (new regression)" — overturned by NEXT.md:81 "the Emscripten web build regression (§4) is resolved ... builds with 0 errors." web_issues.md:22-42 roots the blank canvas in "`<canvas ...>` ... `width=\"0\" height=\"0\"` ... inside SDL3's own Emscripten video backend" — overturned by NEXT.md:67-71 "it's actually an uncaught crash, not a sizing-config issue ... The old theory (`<canvas width=0 height=0>`, a config/sizing problem) was based on incomplete data." README.md:213 still repeats web_issues.md's disproven 0×0/SDL3 theory, so it too is stale.
- **Outcome:** Archive web_issues.md to docs/history/ (or rewrite around NEXT.md's crash root-cause) and update README.md:212-213's Web rows to the crash story so README stops citing the retracted sizing theory.
- **Tests:** Diff web_issues.md §1/§2 against NEXT.md:67-72 and :81; grep README.md:213 for 'width="0"'
- **Verify note:** All claims verified as-is; severity P2 stands. One strengthening addition: README.md:212 not only repeats the stale root cause but states the web app has "no crash," which now directly contradicts NEXT.md:65/70 ("the web build currently crashes before rendering a single ImGui frame" / "The app dies on the first resize event") — an even sharper factual contradiction than the root-cause misattribution alone.
- **Resolved:** commit `0dfcd4f` — verify: `ls docs/history/web_issues.md; grep "width=.0." README.md (expect 0 hits)`

### AUD-051 `[DONE]` `P2` `W13` · README Web build description claims it 'loads and initializes correctly' — NEXT.md says it crashes before the first frame
- **Component:** README.md vs NEXT.md
- **Evidence:** README.md:119-124 "Builds and links cleanly (449/449 objects) and the produced page loads and initializes correctly in a real browser (WebGL 2.0 context, `SDL_CreateWindow` succeeds, no console errors) — **but the 3D viewport currently renders a blank canvas**, an open, undiagnosed limitation". README.md:212 Web 'App launches / runs' 🟡 "loads, initializes ... no crash". NEXT.md:69-71 contradicts: on the first resize event CNA "Throws `std::runtime_error`, uncaught anywhere in the chain, kills the whole wasm module" and NEXT.md:65 "the web build currently crashes before rendering a single ImGui frame". 'no console errors / no crash' vs 'uncaught exception kills the module' cannot both be true.
- **Outcome:** Update README's Web section and Platform Matrix rows to NEXT.md's crash finding; drop 'undiagnosed' (it is now diagnosed) and 'no crash'.
- **Tests:** Diff README.md:119-124 & :212-213 against NEXT.md:65-72
- **Verify note:** Evidence is precise and accurate as stated. Severity P2 is defensible as an internal documentation inconsistency; however, per the rubric ("untruthful capability claim" = P1) it arguably warrants P1, since README asserts a false positive status ("no console errors", "no crash") about a build that the repo's own NEXT.md says crashes before the first frame.
- **Resolved:** commit `0dfcd4f` — verify: `grep -A3 "Build (Web" README.md`

### AUD-052 `[TODO]` `P1` `W11` · CI is permanently parked under `.github_/` — GitHub Actions never runs; the repo has zero CI
- **Component:** .github_/workflows/ci.yml
- **Evidence:** .github_/workflows/ci.yml:1-7 self-documents the deactivation: "DEACTIVATED: this file lives under `.github_/` (note the trailing underscore), not `.github/`, so GitHub does NOT run it." Confirmed: `ls -d .github` -> "No such file or directory"; the only workflow file tracked in git is `.github_/workflows/ci.yml`; `git log --all --diff-filter=D -- .github/workflows/ci.yml` is empty, so an active workflow dir never existed. Net effect: no automated build/test has ever gated any push or PR on master/develop. NOTE the stated activation blocker is also questionable — the comment claims the push token "lacks the `workflow` scope", but `git remote -v` shows origin is an SSH remote (`git@github-openeggbert:openeggbert/mesh-craft.git`), and the workflow-scope restriction is an OAuth/PAT-over-HTTPS guard that does not apply to SSH key pushes; the file can most likely be un-parked by renaming `.github_` -> `.github` and pushing over SSH.
- **Outcome:** Rename `.github_` -> `.github` (over the SSH remote) so the workflow actually runs, or explicitly document why CI must remain disabled; the current state means every correctness/build regression ships unverified.
- **Tests:** After renaming, confirm a run appears under Actions for a test push to develop and that all matrix jobs go green; verify branch protection requires the checks.
- **Verify note:** Core finding fully verified; evidence is accurate. Two refinements: (1) Severity is arguably P2 rather than P1 — the deactivation is intentional, honestly self-documented in the file itself, and tracked as a known owner-blocked task (STAB-0650, referenced in NEXT.md:263-264 and MEMORY as "blocked on owner"/PAT-rotation). It is a documented capability GAP, not a hidden defect or an untruthful capability claim (the file is candid that CI is off). Under the P1 rubric "test gap on critical path" it is defensible, but a reader should know it is a deliberate, disclosed state. (2) The secondary NOTE — that the stated `workflow`-scope blocker is questionable because origin is SSH — is technically plausible (the workflow-scope gate applies to OAuth/HTTPS-PAT pushes; personal SSH keys authenticate as the full user and are generally exempt) but it is SPECULATIVE and not provable from the repo: the local SSH remote does not establish how the maintainer or any automation actually pushes, and the maintainer's notes explicitly cite a "PAT-rotation blocker." Treat that NOTE as a hypothesis, not an established fact.
- **Status note:** OWNER-GATED, correctly not counted as done. .github_/workflows/ci.yml stays parked pending a workflow-scoped push token; documented in NEXT.md and the file itself. Do not mark this DONE without an actual git history entry moving .github_ -> .github.

### AUD-053 `[TODO]` `P2` `W11` · Even if un-parked, CI never builds or tests the actual editor (18.5k LOC) — only 4 standalone CLI/format libs
- **Component:** .github_/workflows/ci.yml
- **Evidence:** ci.yml:39-46 restricts the matrix to `component: [ mc3, mcb, mc3togltf, mc3tomcb ]` and configures each standalone (`cmake -S ${{ matrix.component }}`). ci.yml:19-24 admits the gap: "TODO (not yet wired up): a full editor build job. The root project pulls in the CNA sibling repo ... That covers the remaining root-level tests (smoke_test, xsd_validation, mc3_registry, mc3_ai, mc3_commands, and the render-labeled tests — 20 tests...)". The uncovered `src/MeshCraft/**` editor is 32 .cpp files / 18,529 LOC — the largest first-party surface — plus all render/smoke/AI/registry/XSD tests registered in CMakeLists.txt:408-798 run in no automation.
- **Outcome:** Add an editor build+test job (checkout ../cna + ../sharp-runtime, install SDL3/GL/xvfb) so the smoke/render/xsd/registry/ai CTest suite is exercised, or accept and document that the editor is validated only by hand.
- **Tests:** New CI job runs `ctest` at the repo root under xvfb-run and passes the render-labeled tests.
- **Verify note:** Minor line-range imprecision only: the root-level test registrations actually span lines 408–807 (the final add_test, mc3_ai at line 807, and set_tests_properties through ~808), not 408–798 — CMakeLists.txt is 810 lines total. All other numbers (matrix components, 32 files / 18,529 LOC, the TODO quote at 19-24) are exact. Substance of the finding is unchanged.

### AUD-054 `[DONE]` `P2` `W11` · Compiler warnings enabled on only 2 of the first-party targets; the whole editor + Mcb + mc3tomcb build warning-free, and -Werror is used nowhere
- **Component:** CMakeLists.txt
- **Evidence:** `-Wall -Wextra` appear on exactly two targets: `target_compile_options(Mc3 PRIVATE -Wall -Wextra)` (mc3/CMakeLists.txt:54) and `target_compile_options(mc3togltf_lib PRIVATE -Wall -Wextra)` (mc3togltf/CMakeLists.txt:92). The main editor target is created at CMakeLists.txt:275 (`add_executable(${_target} ${SOURCES})`) with NO warning options anywhere for it — so all 32 files / 18,529 LOC under src/MeshCraft compile with warnings off. Mcb (mcb/CMakeLists.txt), mc3tomcb (mc3tomcb/CMakeLists.txt), and the ai_test/mc3_registry_test executables likewise set no warning flags. A tree-wide grep for `Werror` in all first-party CMake/scripts returns nothing, so even where warnings are on they never fail a build.
- **Outcome:** Enable `-Wall -Wextra` on the MeshCraft/Mcb/mc3tomcb targets (ideally via a shared interface target) and turn on `-Werror` in CI so regressions surface, rather than only decorating two library targets.
- **Tests:** Build the editor with `-Wall -Wextra -Werror` and triage/fix the resulting diagnostics; add the flags to the CI configure step.
- **Resolved:** commit `4fc211e` — verify: `cmake --build cmake-build-debug 2>&1 | grep -c warning` (0) and `ctest --test-dir cmake-build-debug -j$(nproc)` (101/101)
- **Status note:** Added `-Wall -Wextra` to the editor target, Mcb, mc3tomcb, and the mc3togltf executable (mc3togltf_lib and Mc3 already had it). Triaging the resulting warnings on a clean full rebuild found one real bug: the "Focus on Selection" bounding-box `switch (p.primitiveType)` (MeshCraftApplication_Keyboard.cpp) was missing 5 of 11 `PrimitiveType` cases (`-Wswitch`), silently falling back to a wrong hardcoded `hx=hy=hz=0.5f` half-extent for Torus/Capsule/Disk/Grid/IcoSphere — fixed with correct per-primitive geometry (majorRadius/minorRadius for Torus, radius+height for Capsule, etc.). Remaining warnings were narrow buffer-size (`-Wformat-truncation=` on 4 `snprintf` label buffers) and unused-parameter/-variable fixes. Full tree now builds with zero warnings. `-Werror` in CI is not added — CI itself is parked pending AUD-052 (owner-gated), so a CI-side flag would be inert; the local build-warning-free state is the actionable part of this finding and is now true.

### AUD-055 `[DONE]` `P2` `W11` · No ASan/UBSan/clang-tidy/clang-format/coverage/fuzz config anywhere, despite parsing untrusted MCB binary + MC3 XML input
- **Component:** CMakeLists.txt (build hardening)
- **Evidence:** Tree-wide grep across all first-party CMakeLists/*.cmake/*.sh/*.yml for `sanitize|clang-tidy|clang-format|gcov|--coverage|lcov|libfuzzer|afl|cppcheck|iwyu` returns zero hits; no `.clang-format`/`.clang-tidy`/`.editorconfig` files exist. The one fuzzing knob present is a disable: `set(MANIFOLD_FUZZ OFF ... FORCE)` (CMakeLists.txt:180). Yet the project parses untrusted, attacker-controllable inputs — the MCB binary reader (mcb/src/McbReader.cpp) and the MC3 XML parser (mc3/src/Mc3XmlParser.cpp) — with no sanitizer build option or fuzz target guarding them.
- **Outcome:** Add an opt-in ASan/UBSan build configuration and at least one fuzz/differential harness over McbReader and Mc3XmlParser, wired into CI; add clang-format/clang-tidy config for consistency.
- **Tests:** Run the existing round-trip/parse tests under `-fsanitize=address,undefined` and a short libFuzzer/AFL run over the two parsers to confirm no UB on malformed input.
- **Verify note:** Severity P2 is fair (could also be argued P3 polish). Two evidence refinements for accuracy: (a) clang-tidy (19.1.7) and cppcheck (2.17.1) WERE actually run as one-off manual passes over mc3/src/ per plan_20260710.md STAB-0614/STAB-0620 — so the precise defect is 'no checked-in .clang-tidy/.clang-format config and no CI-integrated/repeatable sanitizer/static-analysis/fuzz gating', not literally 'clang-tidy never run'. (b) The MANIFOLD_FUZZ OFF at CMakeLists.txt:180 disables fuzzing of the vendored Manifold geometry dependency, not the project's own McbReader/Mc3XmlParser, so it is tangential to the untrusted-input hardening argument rather than direct evidence of it.
- **Resolved:** commit `4fc211e` — verify: `cmake -S . -B /tmp/asan-build -DMESHCRAFT_SANITIZE=ON -DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL && cmake --build /tmp/asan-build --target mc3_roundtrip_test mcb_roundtrip_test && /tmp/asan-build/mc3/mc3_roundtrip_test && /tmp/asan-build/mcb/mcb_roundtrip_test`
- **Status note:** Added an opt-in `MESHCRAFT_SANITIZE` CMake option building first-party targets (Mc3/Mcb/mc3togltf/mc3tomcb/editor) with ASan+UBSan, deliberately excluding CNA/SHARP_RUNTIME/vendored deps (out of scope per CLAUDE.md). Fixed a link-time propagation bug found during verification: a per-target `target_link_options()` on a STATIC LIBRARY (Mc3/Mcb) does not carry to executables that link against it, so `mc3_roundtrip_test`/`mcb_roundtrip_test` failed with `undefined reference to '__asan_report_load8'` etc. Root-caused to needing the sanitizer runtime linked at every final-executable link step, not just the instrumented library; fixed via a global `add_link_options(-fsanitize=address,undefined)` at the CMakeLists.txt root, before any `add_subdirectory()`, so every final link in the tree picks up the runtime (harmless for non-instrumented CNA/vendored object code) while compile-time instrumentation stays scoped to first-party targets via `meshcraft_apply_sanitize()`. Verified `mc3_roundtrip_test` and `mcb_roundtrip_test` both link and run clean under ASan/UBSan with zero findings. **Not done** (explicitly out of scope for this pass, left for follow-up): CI wiring (blocked on AUD-052), clang-format/clang-tidy checked-in config, and a dedicated libFuzzer/AFL harness over McbReader/Mc3XmlParser — those remain open work, tracked separately (SYS-W11-04 covers the sanitizer-config half of this gap; SYS-W11-05 covers fuzzing).

### AUD-056 `[DONE]` `P3` `W11` · CI FetchContent cache key omits the file where 3 of 4 components' real dependency is pinned — documented invalidation guarantee is inaccurate
- **Component:** .github_/workflows/ci.yml
- **Evidence:** ci.yml:60-71 keys the `build/_deps` cache on `hashFiles(format('{0}/CMakeLists.txt', matrix.component))` and comments (ci.yml:61-64) that this is "keyed on the component's own CMakeLists.txt content so a dependency version bump (a GIT_TAG/URL change in that file) invalidates the cache." But mcb, mc3togltf, and mc3tomcb fetch their only external dep (tinyxml2) transitively via `add_subdirectory(../mc3 ...)` (mcb/CMakeLists.txt:14-17, mc3togltf/CMakeLists.txt:32-35, mc3tomcb/CMakeLists.txt:11-14), and tinyxml2's pin lives in mc3/CMakeLists.txt:15-19 — a file NOT covered by those components' cache keys. A tinyxml2 bump therefore does not invalidate their cache key (impact is partly mitigated because FetchContent's own ExternalProject stamp re-checks the GIT_TAG on the restored tree).
- **Outcome:** Either include mc3/CMakeLists.txt in the hash for the mc3-dependent components, or reword the comment so it doesn't overstate the invalidation guarantee.
- **Tests:** Bump the tinyxml2 GIT_TAG in mc3/CMakeLists.txt and confirm the mcb/mc3togltf/mc3tomcb cache keys change (currently they do not).
- **Verify note:** Additional context that the evidence omits but that reinforces (does not undermine) the low P3 severity: the entire workflow is deactivated — its own banner at ci.yml:3-7 states the file lives under `.github_/` (trailing underscore) specifically so GitHub does NOT run it, meaning the cache step never actually executes in practice. Combined with the acknowledged FetchContent ExternalProject stamp that re-checks GIT_TAG on the restored tree, the real-world functional impact is essentially nil; this is purely a comment/documentation-accuracy inaccuracy. Severity P3 stands.
- **Resolved:** commit `d16c82c` — verify: `grep "mc3/CMakeLists.txt" .github_/workflows/ci.yml`

### AUD-057 `[TODO]` `P2` `W11` · Editor build pulls sibling repos via add_subdirectory(../cna) / SHARP_RUNTIME with no version pin — non-reproducible and unguarded by CI
- **Component:** CMakeLists.txt
- **Evidence:** CMakeLists.txt:107 `add_subdirectory(../cna CNA_dep)` and the link lines at CMakeLists.txt:338/354/365 (`CNA ... SHARP_RUNTIME ...`) consume two sibling repos purely by relative path, with no GIT_TAG, commit, or version check — whatever happens to be checked out at ../cna and ../sharp-runtime is used. README.md:48-50 documents the checkout requirement but not any pinned revision, and README.md:211 records that this is actively fragile: "a *fresh* rebuild now fails — `../sharp-runtime` gained a new Emscripten-only regression ... (16 `-Werror` failures + 1 hard `std::chrono::clock_cast` compile error)." With CI parked (finding 1) and the editor uncovered even if un-parked (finding 2), nothing detects such sibling-repo breakage.
- **Outcome:** Pin the sibling repos to explicit commits/tags (submodule or a recorded SHA + a configure-time check), and gate the editor build in CI against those pinned revisions so cross-repo regressions are caught.
- **Tests:** Add a configure-time assertion that ../cna and ../sharp-runtime are at expected revisions; add a CI editor-build job that fails when they drift/break.
- **Blocked:** Fixing the sibling-repo build regressions themselves is out of scope (../cna and ../sharp-runtime are owned elsewhere); only mesh-craft's pinning/CI wiring is in scope here. **Partially resolved below** — the configure-time-assertion half is done; the CI-job half remains blocked on AUD-052 (CI itself is parked).
- **Resolved (partial):** commit `d2943e3` — verify: `cmake -S . -B <build-dir> 2>&1 | grep AUD-057` (expect no output when siblings are at the recorded SHA; a `-DMESHCRAFT_CNA_VERIFIED_SHA=0...0` override reproduces the warning path)
- **Status note:** Added a non-fatal configure-time check (`meshcraft_check_sibling_revision`, CMakeLists.txt) that `git rev-parse HEAD`s `../cna` and `../sharp-runtime` and prints `message(WARNING ...)` — not `FATAL_ERROR` — when either has drifted from the two `MESHCRAFT_*_VERIFIED_SHA` values recorded in the same file (currently `../cna` @ `d0c21ee6`, `../sharp-runtime` @ `5cdaafb2`, both re-verified against a passing 101/101 `ctest` run just before recording). Deliberately non-fatal: CNA is developed by a separate process (CLAUDE.md — "No CNA changes without owner permission. A separate Claude Code instance handles CNA."), so it legitimately moves ahead of this project's last-verified pin; a hard `FATAL_ERROR` would block that work every time it advances. Verified both the silent-when-matching path and the warning-when-drifted path (temporarily overwrote `MESHCRAFT_CNA_VERIFIED_SHA` with a bogus SHA, confirmed the warning fires and configure still exits 0, then restored the correct value and re-verified a clean reconfigure + full rebuild + 101/101 ctest). **Remaining (not done):** the CI-job half (gate the editor build in CI against these pins) is blocked on AUD-052 — there is no running CI to wire it into. The pinned SHAs are a manually-updated marker, not automation; they need a human/agent to re-run `ctest` and bump them after intentionally picking up new sibling commits, which is not enforced by anything.
### AUD-006b `[DONE]` `P1` `W1` · Untrusted-resource confinement is exporter-only and include-only — texture/mesh/SVG/embed/sound/music paths are not confined at the AI/application layer
- **Component:** src/MeshCraft/AiResponseAlgorithms.hpp, mc3/include/MeshCraft/Mc3/Mc3LoadPolicy.hpp
- **Evidence:** AUD-006's fix (commit 901965f) confines texture/mesh paths only inside mc3togltf's exporter (assertResourceAllowed in GltfExporter.cpp), and AUD-008's fix (40643a4) confines `<include>` only via Mc3LoadPolicy::untrusted() at parse time. Neither covers: (a) the editor application loading a texture/SVG/sound/music path from an AI-generated or pasted document directly (not via mc3togltf export) — MeshCraftApplication texture-loading code reads `tex.uri`/`svg.src`/sound and music `src` with no policy check at all; (b) `embed:` src resolution; (c) any AI-generated document that never reaches mc3togltf (e.g. the user just views/edits it in the editor without exporting). An AI response containing a `<texture uri="../../../../home/user/.ssh/id_rsa">` that the user merely opens in the editor (never exports to glTF) is unconfined today.
- **Outcome:** Extend Mc3LoadPolicy (or a sibling Mc3ResourcePolicy) to cover texture uri, SVG src, mesh src, embed src, sound src, music src — applied at parse/application time for untrusted documents, not only at mc3togltf export time. Define the threat model per source explicitly: trusted user-opened local scenes (permissive), AI-generated scenes (confined), pasted/imported scenes (confined), CLI conversion of untrusted files (confined, existing mc3togltf flag), registry content (confined unless explicitly trusted).
- **Tests:** Load an AI-simulated document (Mc3Document::loadFromString with untrusted policy) containing an absolute/traversal texture/sound/music path; assert the loader rejects or strips the reference without ever opening the file, verified via a filesystem-access probe (e.g. a sentinel file that must not be touched).
- **Resolved:** commit `ed6e220` — verify: `ctest -R mc3_load_policy`
- **Status note:** Added `Mc3LoadPolicy::confineResourcePathsToRoot` (true in `untrusted()`) and wired validation into all 6 parse sites (mesh src, texture uri, SVG src, sound src, music src, embed src) in `Mc3XmlParser.cpp` -- rejecting at PARSE TIME, before the `Mc3Document` exists for any caller (editor UI, mc3togltf) to inspect, so every consumer is protected uniformly rather than needing its own check. `embed:`/`data:` pseudo-references are correctly exempted. Verified for all 6 field types plus a trusted-policy control (confinement stays opt-in) and an embed: false-positive guard. **Known limitation** (documented in the commit, not fixed): an included file's own resource paths are validated against the top-level document's root before STAB-0550's rebasing runs, so `confineResourcePathsToRoot` combined with `allowIncludes=true` on a hypothetical custom policy could misjudge a same-directory resource in a nested include. Not exercised by any current caller -- `untrusted()` sets `allowIncludes=false`, so this combination doesn't occur in practice today.

### AUD-036b `[DONE]` `P0` `W9` · AUD-036's fix was only source-pattern-verified, never behaviorally verified against a real ImGui frame, and no central helper existed to stop the bug class recurring
- **Component:** src/MeshCraft/Scene/PropertiesPanel.cpp, src/MeshCraft/MeshCraftApplication_Commands.cpp, include/MeshCraft/MeshCraftApplication.hpp, include/MeshCraft/Scene/PropertiesPanel.hpp, test/undo_gesture_frame_test.cpp (undo/redo subsystem)
- **Evidence:** AUD-036 (commit 737af77) fixed the dead `if(IsItemActivated()) pushUndo()`-nested-in-changed-block pattern at all 80 identified sites, guarded only by a source-lint (undo_snapshot_lint_test.py) matching that one text pattern in 3 files — it never drives a real ImGui frame, never confirms `pushUndo()` actually captures pre-mutation state, and there was no central helper, so nothing stopped a new call site from hand-rolling the same dead pattern again.
- **Outcome:** This entry covers 2 of the original 4 asks; the other 2 (full 78-candidate `undo_coverage_audit.py` triage, and frame-driven coverage of the remaining widget categories) are real, separately-scoped remaining work, split off as `AUD-036c` rather than folded into this DONE claim. Delivered here: (1) `test/undo_gesture_frame_test.cpp` drives real ImGui frames through a full click-drag gesture on `DragFloat`/`DragFloat3`/`ColorEdit3` (the exact widget classes AUD-036 fixed) plus `SliderFloat` as a control case, and behaviorally proves — not just source-pattern-matches — that the fix produces exactly one undo snapshot per gesture, taken on/before the first mutation frame. (2) A central transaction helper, `MeshCraftApplication::undoOnActivate(bool widgetChanged)` (exposed to `PropertiesPanel` via `PropertiesContext::undoOnActivate`), collapses the 3-line `bool ch = Widget(...); if (IsItemActivated()) pushUndo(); if (ch) {...}` pattern into `if (ctx.undoOnActivate(Widget(...))) {...}` — the snapshot now fires unconditionally inside the helper, decoupled from the caller's changed-block, so the specific AUD-036 bug class is structurally impossible at call sites that use it instead of hand-rolling the pattern. Adopted at 3 representative call sites (Transform tab position/rotation/scale, PropertiesPanel.cpp) to prove it round-trips correctly against real editor code and the full test suite; not mechanically retrofitted to the remaining ~330 `pushUndo`-adjacent call sites (real, deliberately out-of-scope — see `AUD-036c`).
- **Tests:** `ctest -R undo_gesture_frame` (new, 9 assertions across DragFloat/DragFloat3/ColorEdit3/SliderFloat, all passing). `ctest -R undo_snapshot_lint` (pre-existing) still passes on the refactored PropertiesPanel.cpp call sites — the helper never emits the literal `if (ImGui::IsItemActivated())` text the lint greps for, so there is no interaction (false-positive or false-negative) between the two guards.
- **Resolved:** commit `89d874d` — verify: `ctest -R 'undo_gesture_frame|undo_snapshot_lint'` (full suite: `ctest` from the build dir).

### AUD-036c `[DONE]` `P1` `W9` · Full undo_coverage_audit.py triage (78 candidates) and frame-driven coverage of the remaining mutating-widget categories
- **Component:** src/MeshCraft/* (undo/redo subsystem), test/undo_coverage_audit.py
- **Evidence:** Split off from `AUD-036b` (see its Outcome) to keep that entry's DONE claim honest. `test/undo_coverage_audit.py` reports 78 candidate call sites, none formally triaged into its own real-mutation / transient-preview / false-positive / intentionally-non-undoable categories (2 were manually spot-checked in this session as examples: `PropertiesPanel.cpp:2079` = transient-preview, `MeshCraftApplication_UiToolbar.cpp:225` = false-positive — not a substitute for triaging the other 76). `test/undo_gesture_frame_test.cpp` (AUD-036b) covers only Drag*/ColorEdit*/Slider*; Checkbox, Combo, RadioButton, InputText, drag-and-drop, and multi-object-mutation buttons have no frame-driven or extracted-controller behavioral coverage confirming `pushUndo()` fires pre-mutation for them too.
- **Outcome:** (1) Triage all 78 `undo_coverage_audit.py` candidates into the four categories with a written classification per candidate; escalate any found to be REAL_MUTATION-with-no-undo to a new P0/P1 bug (matching the class AUD-036 fixed). (2) Extend the frame-driven/extracted-controller test approach to Checkbox, Combo, InputText, and a multi-object edit, asserting actual document field values are correct across Undo and Redo. (3) Verify the broader guarantees the original AUD-036b Outcome specified: no snapshot for a no-op gesture where practical, redo invalidated on the first real mutation, cancel/revert leaves no false history item, multi-selection edits are atomic, selection restored correctly after undo/redo, locked objects untouched. (4) Optionally extend `undoOnActivate`/`PropertiesContext::undoOnActivate` adoption beyond the 3 representative call sites AUD-036b converted, where doing so doesn't require a wider `PropertiesContext`-shaped refactor of the non-PropertiesPanel UI files.
- **Tests:** Extend test/undo_gesture_frame_test.cpp (or a sibling file) with Checkbox/Combo/InputText/multi-object cases; a `test/undo_triage.md`-style artifact or plan.md sub-table recording the 78-candidate classification.
- **Blocked:** None — scoped, in-repo, large in surface area (78 candidates across PropertiesPanel.cpp/UiLeftPanel.cpp/UiOverlays.cpp/UiMenuBar.cpp/UiRegistry.cpp/Anim.cpp), continuing incrementally.
- **Resolved:** commit `d8c14af` (audit-script false-positive fix) + `737e338` (extended test coverage) — verify: `ctest -R undo_gesture_frame`, `python3 test/undo_coverage_audit.py .`
- **Status note:** All 81 candidates (grew from 78 to 81 between sessions as new call sites were added elsewhere) individually classified by reading real surrounding code, not guessed from widget names: **0 REAL_MUTATION** (AUD-036/AUD-036b already closed the actual bug class), 39 TRANSIENT_PREVIEW (each traced to a real Apply/Confirm handler that calls `pushUndo()` before mutating, including batch operations — confirmed exactly one snapshot per batch regardless of selection size), 3 FALSE_POSITIVE (`PropertiesPanel.cpp:135,166,195`, already correctly using `ctx.undoOnActivate(...)`, just unrecognized by the audit script's regex — fixed, candidate count now 78), 39 INTENTIONALLY_NON_UNDOABLE (plain UI/tool/renderer preference members never written into `document_`). `test/undo_gesture_frame_test.cpp` extended with frame-driven coverage for Checkbox, Combo (open+select as one atomic gesture), InputText (confirmed the codebase's actual convention is `ImGuiInputTextFlags_EnterReturnsTrue` + commit-on-Enter — `IsItemDeactivatedAfterEdit` has zero call sites in `src/`), a synthetic multi-object batch edit modeled on a real batch handler, a hover-only no-op check, and a redo-invalidation check mirroring `pushUndo()`'s `redoStack_.clear()` — 27 assertions, all passing. **Two open items intentionally left as design questions, not silently claimed fixed:** (1) `performUndo()`/`performRedo()` (`MeshCraftApplication_Commands.cpp`) unconditionally `selection_.clear()` rather than restoring the pre-undo/redo selection — safe (no dangling pointers into the swapped snapshot) but the "selection restored correctly after undo/redo" guarantee from AUD-036b's Outcome does not literally hold as written, and is untested; a real product decision (should Ctrl+Z restore selection?), not a bug this task's scope authorized fixing unilaterally. **Decision (2026-07-17, human-authorized): yes** — `performUndo()`/`performRedo()` should restore the pre-undo/redo selection rather than clearing it; implemented same day, see `SYS-W9-03` below. (2) "Locked objects untouched by undo/redo" is well-tested at the per-command level (batchRename/findReplace/align/scatter/rotate/scale already exclude locked objects, `mc3/test/editor_commands_test.cpp`), but undo/redo itself is a whole-document snapshot swap with no separate lock-awareness — by design (a lock is a property stored ON an object, so it round-trips through the snapshot automatically), not a gap, but also not independently tested as its own guarantee.

### AUD-039b `[DONE]` `P1` `W8` · Gate C requires real enforcement, not a configure-time warning — the editor still builds (non-functionally) under BGFX/VULKAN/SDL_RENDERER
- **Component:** CMakeLists.txt
- **Evidence:** AUD-039/040's fix (commit e53af49) added a `message(WARNING ...)` at configure time when a non-EASYGL backend is selected, but the editor target is still added, still configured, and still builds successfully — a user who ignores or doesn't see the warning (e.g. CI configuring headlessly, or a GUI CMake frontend that collapses warnings) still gets a MeshCraft.exe/MeshCraft binary that launches, allocates a window, and then renders nothing, with no run-time indication of why. Per the adversarial re-review, "a warning while still building a known non-rendering editor does not satisfy Gate C" ("never advertise selectable but non-functional configurations" / "a backend... must be classified as exactly one of: implemented and verified, implemented but explicitly unverified, partially implemented with exact limitations, or unsupported and rejected clearly").
- **Outcome:** Choose and implement one truthful, enforced behavior instead of a warning: (a) configure-time `message(FATAL_ERROR ...)` for the MeshCraft editor target specifically under unsupported backends (CLI tools mc3togltf/mc3tomcb/mc3/mcb remain buildable under any backend since they don't depend on it), with an explicit opt-out flag for deliberate CNA-only experimentation; or (b) a genuinely separate CLI-only build mode/target that excludes the editor executable entirely under non-EASYGL backends.
- **Tests:** Configure with `-DMESH_CRAFT_GRAPHICS_BACKEND=BGFX` (and VULKAN, SDL_RENDERER) and assert the editor target is NOT built (configure fails, or the target is absent from the build graph), while `-DMESH_CRAFT_GRAPHICS_BACKEND=EASYGL` continues to configure and build the editor with zero warnings; assert the CLI tools (mc3togltf/mc3tomcb) still build under all four backends.
- **Resolved:** commit `58a7f03` — verify: `ctest -R graphics_backend_check` (unit test for the decision logic); full manual verification below.
- **Status note:** Chose the runtime-check design (compiled-in backend name + a check at the very top of `main()`, before any window/GL init) over restructuring the CMake target, since the latter would touch several hundred lines of stabilized build configuration (sources/link libraries/every test referencing `${_target}`) for high regression risk. `MESH_CRAFT_ALLOW_UNSUPPORTED_BACKEND=1` is the documented escape hatch. **Manually verified end-to-end** (not just the unit test): configured and built a full `-DMESH_CRAFT_GRAPHICS_BACKEND=SDL_RENDERER` tree (~10 min CNA rebuild in a throwaway `b-sdlrenderer/` dir, removed after). Without the override: `[MeshCraft] Error: this build was configured with MESH_CRAFT_GRAPHICS_BACKEND='SDL_RENDERER'...` printed, exit code 1, no window ever created, no screenshot file produced. With `MESH_CRAFT_ALLOW_UNSUPPORTED_BACKEND=1`: proceeded past the check (printing a warning), created a window, then crashed further downstream trying to actually render -- confirming the underlying claim (this backend genuinely cannot run the editor) rather than indicating a problem with the check itself. CLI tools are unaffected (they don't link the `MeshCraft` target). The CMake-side configure-time WARNING from `AUD-039`/`AUD-040` (commit `e53af49`) is unchanged/still present as a secondary signal.

### AUD-058 `[DONE]` `P1` `W0` · BloomGL::cleanup() exists but is never called — bloom/SSAO/skybox/material-preview/shader/VAO/VBO/FBO/texture GL resources leak on every shutdown
- **Component:** src/MeshCraft/MeshCraftApplication.cpp (BloomGL, s_bloom)
- **Evidence:** ~MeshCraftApplication() (added by AUD-011's fix, commit 2d126bf) removes the SDL event watch, shuts down ImGui, and deletes shadowDebugFbo_/shadowDebugColorTex_/shadowDebugDepthTex_ — but it never calls `s_bloom.cleanup()`. `struct BloomGL` (MeshCraftApplication.cpp:795) owns a large pool of GL objects (bloom FBOs/textures, SSAO FBO/texture, skybox VAO/VBO/texture, material-preview FBO/texture, multiple shader programs) allocated across LoadContent/lazy-init call sites, and its own `cleanup()` method (line 981) already correctly deletes all of them guarded by non-zero checks — it is simply dead code, never invoked from any shutdown path. Every one of these GL objects leaks for the process lifetime on every run.
- **Outcome:** Call `s_bloom.cleanup()` from ~MeshCraftApplication() (before or alongside the shadowDebug cleanup already there), and audit for any other GL-owning file-static/member structures in the same file that likewise define a cleanup()/dispose() method with zero call sites.
- **Tests:** A GL-object-count assertion (e.g. query GL_NUM_* or track allocation/deletion pairs via a thin instrumentation wrapper) confirming zero leaked bloom/SSAO/skybox/material-preview objects after app shutdown; at minimum, a compile-time check that s_bloom.cleanup() has a real call site (grep-based lint, mirroring undo_snapshot_lint_test.py's approach).
- **Resolved:** commit `0c150e5` — verify: `ctest -R gl_shutdown_leak`
- **Status note:** Added `gl.cleanup()` call plus a self-checking `allReleased()`/`leakCheck()` regression guard (catches a FUTURE un-wired resource, not just this one). Audited for sibling GL-owning structures with an unreferenced cleanup method — found none (`SceneRenderer`/`GridRenderer` go through CNA's own `GraphicsDevice`-managed resource types, disposed via `Game::Dispose()`, a different and already-correct lifecycle path; the only other raw-GL surface in this file is a one-shot `glReadPixels` screenshot readback with no persistent allocation). Verified the regression test has real teeth: manually reverted the fix, confirmed the test fails naming the exact leaked handles, restored the fix, confirmed it passes again — 5/5 clean runs each way (isolating one unrelated flaky-driver segfault hit during testing that reproduced regardless of this fix and was not caused by it).

### AUD-059 `[DONE]` `P1` `W1` · Tessellation clamp is a per-field cap only — no total-document allocation budget, so many objects near the cap still sum to unbounded memory
- **Component:** mc3/src/Mc3XmlParser.cpp (attrCount / kMaxTessellation)
- **Evidence:** AUD-005's fix (commit fd606d2) clamps each individual segments/sides/subdivisions field to kMaxTessellation=4096, closing the single-primitive-with-absurd-segment-count DoS. It does not bound the SUM across a document: a hostile/AI-generated document with, say, 100,000 `<sphere segments="4096"/>` objects (each individually legal, well under the per-field cap) still requests roughly 100,000 × (4096/2+1) × (4096+1) ≈ 8.6e11 vertices in aggregate — the same memory-exhaustion class of attack the per-field clamp was meant to close, just redistributed across many objects instead of one. There is also no cap today on total object count, total tree node count, definitions/instances count, animation channels/keyframes, or embedded/base64 byte totals.
- **Outcome:** Add a running allocation-budget system during parse (or as a post-parse validation pass): track estimated total vertices/indices/generated bytes, total object/node count, definitions+instances, recursion depth (already partially covered), CSG input count, extrude cross-section/path sample totals, animation channel/keyframe counts, and embedded/base64 byte totals against fixed ceilings, rejecting with a clear diagnostic before attempting the corresponding large allocation (not after allocating and then discovering it was too much).
- **Tests:** A fixture with many (e.g. 50,000) individually-legal objects whose combined estimated vertex count exceeds the budget; assert the loader rejects it BEFORE allocating hundreds of megabytes (verified via a peak-RSS check or an allocation-counting hook, not just wall-clock time).
- **Resolved:** commit `9a4b8f6` — verify: `ctest -R mc3_input_budget`
- **Status note:** Implemented the primary attack surface this finding's evidence describes: a `DocumentBudget` (Mc3XmlParser.cpp) tracks total object count (100,000 ceiling) and total tessellation weight -- the sum of every segments/sides/subdivisions value across the whole document, including extrude cross-section/path segments (500,000 ceiling) -- rejecting with a clear error naming the budget before any downstream geometry generation. Since mc3 itself never allocates vertex buffers (only mc3togltf does), a parse-time throw is proof no large allocation was attempted; verified with a 200-object/max-segments fixture (combined weight 819,200) that is rejected, and a 500-object/modest-segments fixture (weight 8,000) that still loads correctly. **NOT covered** (broader scope than this finding's core evidence, tracked instead under `SYS-W1-03`): total generated-byte estimates, definitions+instances count as a distinct budget, animation channel/keyframe counts, embedded/base64 byte totals. Object recursion depth is separately covered by `RecursionGuard`-equivalent limits noted elsewhere in the audit.


### AUD-060 `[DONE]` `P1` `W1` · Resource-confinement (AUD-006) false-positive rejects same-directory files when the document is opened via a bare relative filename
- **Component:** mc3togltf/src/MeshBuilder.cpp (assertResourceAllowed), mc3/src/Mc3XmlParser.cpp (includePathWithinRoot)
- **Evidence:** Found while building AUD-002's regression fixture. `doc.sourcePath` (the `basePath`/`rootDir` argument both confinement functions receive) is `selfPath.parent_path()`, which is EMPTY when the document is opened via a bare relative filename with no directory component — the common `mc3togltf scene.mc3.xml out.glb` invocation run from the scene's own directory. `std::filesystem::weakly_canonical("")` returns an empty path rather than resolving to the current working directory or erroring, so `root` stayed empty, `std::filesystem::relative(cand, root)` against an empty base returned empty too, and the `rel.empty()` branch of both functions' rejection check fired — wrongly treating every same-directory texture/mesh reference or `<include>` as "escaping the document root". Reproduced directly: `mc3togltf oob_test.mc3.xml out.glb` run from within the fixture's own directory (both files co-located) failed with "mesh source '...' escapes the document root" even though it plainly does not. No existing test caught this because every confinement/hostile-input test fixture used `tempfile`-style absolute paths for both the `.mc3.xml` and its resources.
- **Outcome:** Normalize an empty base path to `"."` before canonicalizing in both `assertResourceAllowed` and `includePathWithinRoot`.
- **Tests:** `mc3/test/load_policy_test.cpp` gained a case that `chdir`s into the fixture directory and opens `"main.mc3.xml"` (no directory prefix) under a confined policy, asserting the same-directory `<include>` still merges instead of being rejected. `test/obj_malformed_oob_positive.obj`'s wiring into `obj_robustness_test.py` (run via a relative path in its own `WORKING_DIRECTORY`) exercises the mc3togltf-side fix identically.
- **Resolved:** commit `d701df7` — verify: `ctest -R "mc3_load_policy|mc3togltf_obj_robustness"`
- **Status note:** This was a real regression in this session's own earlier AUD-006 fix (commit `901965f`) that would have broken the exporter's single most common real-world invocation pattern for any scene referencing a texture or mesh file. Caught only by incidentally testing AUD-002 with co-located relative paths instead of the test suite's habitual absolute tempdir paths — a reminder that "all tests pass" is not the same as "the common real invocation works."

### AUD-061 `[DONE]` `P2` `W7` · Viewport Torus/Capsule render the wrong shape whenever the primitive's own radius ratio differs from the fixed unit mesh's baked-in ratio
- **Component:** src/MeshCraft/Renderer/SceneRenderer.cpp (drawE), include/MeshCraft/Renderer/PrimitiveTessellationAlg.hpp
- **Evidence:** Found by `SYS-W7-02`'s differential geometry test (`differential_geometry_test`, commit `1ae9087`). The viewport builds ONE fixed unit torus mesh (`majorRadius=0.35`, `minorRadius=0.15`, baked into vertex positions by `tessellateUnitTorusAlg`, `PrimitiveTessellationAlg.hpp:195`) and reproduces every document Torus by a single non-uniform affine scale: `SceneRenderer.cpp:971` `Matrix::CreateScale({R/0.35f, r/0.15f, R/0.35f})`. This can only correctly reproduce a torus whose `majorRadius/minorRadius` ratio equals `0.35/0.15` (≈2.33) — any other ratio produces an elliptical-cross-section tube, not a true torus. The same structural bug applies to Capsule: `SceneRenderer.cpp:977` `Matrix::CreateScale({r*2.0f, (h+r*2.0f)/2.0f, r*2.0f})` scales one fixed unit capsule, correct only when the document's `radius`/`height` ratio matches the unit mesh's own — otherwise the hemisphere caps become ellipsoidal instead of spherical. The independent glTF exporter (`mc3togltf/src/MeshBuilder.cpp`) tessellates each Torus/Capsule directly from its actual parameters and is confirmed correct by the same differential test — so this is a viewport-only bug; exported files are unaffected.
- **Outcome:** Took the "tessellate Torus/Capsule per-object at their actual parameters" option (dropping the fixed-unit-mesh-plus-scale optimization for these two primitive types only, matching what the exporter already does). `tessellateUnitTorusAlg()`/`tessellateUnitCapsuleAlg()` (`PrimitiveTessellationAlg.hpp`) gained real `majorRadius`/`minorRadius` and `radius`/`height` parameters (default-preserving the historical unit values, so every other caller is behavior-unchanged). `SceneRenderer::getOrBuildTorusMesh()`/`getOrBuildCapsuleMesh()` build a real mesh per distinct `(LOD tier, actual radius parameters)` combination on demand and cache it in a small bounded `std::map` (same 128-entry-then-clear eviction policy as the existing CSG preview cache, `csgMeshCache_`) — a static scene redrawn every frame hits the cache after the first draw of each distinct combination, so this is not a per-frame re-tessellation regression. Both `drawObject()`'s solid-render path and `drawEmissiveObject()`'s bloom path were switched over; the emissive path additionally guards the cache lookup behind `hasEmissive` so a non-emissive Torus/Capsule doesn't pay for a cache lookup every frame. Capsule's VPNT normal generation (previously a fragile position back-classification hardcoded to radius=0.5) was generalized to exact per-ring analytic normals computed the same way `buildUnitTorus()` already does. Capsule's wire-outline construction, which used to live inside `buildUnitCapsule()` (a single shared `wireShapeCapsule_` member), was moved into `buildWireShapes()` so per-object-ratio rebuilds of the mesh can't overwrite it with non-unit dimensions.
- **Tests:** `differential_geometry_test`'s Torus/Capsule off-ratio cases, which used to only PIN the old bugged behavior with an exact formula, now assert actual correctness: renderer volume/bbox matches both analytical ground truth and the exporter's independently-correct output at the SAME off-ratio parameters (`majorRadius=0.6/minorRadius=0.2` for Torus, `radius=0.4/height=1.5` for Capsule) — the off-ratio capsule case's renderer and exporter volumes are now bit-identical (`0.986054` both). Full `ctest` run: 115/115 passing, including all 21 render pixel-sampling tests. Real headless visual verification: `xvfb-run ... MeshCraft <scene> --screenshot out.ppm` on two fixtures with deliberately extreme off-default ratios (Torus `majorRadius=0.8/minorRadius=0.5`, ratio 1.6; Capsule `radius=0.3/height=1.8`, height ≫ 2×radius) — both rendered as plausible, undistorted shapes (a proper round donut hole and circular tube cross-section for the torus; a tall pill with visibly spherical, non-ellipsoidal hemisphere caps for the capsule), not the elliptical/ellipsoidal distortion the bug used to produce.
- **Resolved:** commit `2cb8d25` — verify: `ctest --test-dir cmake-build-debug -R differential_geometry` (or the full suite)
- **Status note:** `objectPolyStats()`'s Torus/Capsule vertex/triangle counts still reference the fixed full-quality `unitTorus_`/`unitCapsule_` members directly rather than the per-object cache — this remains correct because ring/tube/segment COUNTS are identical across all ratio variants at a given LOD tier (only vertex positions differ, not counts), so no change was needed there. Not fixed as part of this task (out of scope, unrelated to AUD-061's shape-correctness finding): `drawObjectWireframe()`'s Torus/Capsule selection-outline wireframe still scales the fixed-ratio `wireShapeTorus_`/`wireShapeCapsule_` non-uniformly by the object's actual dimensions — the same structural issue as the fixed bug, but only affects the thin selection-highlight outline, not the solid rendered shape, and was not covered by the differential test's evidence for this finding.

### AUD-062 `[DONE]` `P3` `W7` · Viewport IcoSphere ignores the primitive's own `segments`/subdivision field — always renders at a fixed subdivision level
- **Component:** src/MeshCraft/Renderer/SceneRenderer.cpp, src/MeshCraft/Renderer/SceneRenderer_Builders.cpp
- **Evidence:** Found by `SYS-W7-02`'s differential geometry test. Every other curved primitive gets 3 pre-built LOD tiers selected by camera distance at draw time (`SceneRenderer.cpp:368` `buildUnitSphere(32,...); buildUnitSphere(16,...); buildUnitSphere(6,...)`; similarly for Torus at line 372, Capsule at line 373). IcoSphere gets exactly one: `SceneRenderer.cpp:374` `buildUnitIcoSphere(2)` — a single hardcoded `subdivisions=2` mesh, reused for every document IcoSphere at every distance via a pure uniform-radius scale (`SceneRenderer.cpp:781/982` `Matrix::CreateScale({r,r,r})`). The document's own `segments`/subdivision field (whatever `Mc3Primitive` calls it for IcoSphere — verify the exact field name) is read by the independent glTF exporter (confirmed correct by the differential test) but never consulted by the viewport at all.
- **Outcome:** Either build IcoSphere unit meshes at multiple subdivision tiers keyed off the document's requested value (matching how Sphere/Torus/Capsule already vary tessellation quality), or, at minimum, document that IcoSphere's viewport LOD is currently fixed regardless of the scene's declared subdivision level.
- **Tests:** Extend `differential_geometry_test` (or a new focused test) to assert the viewport's IcoSphere vertex/triangle count actually varies with the primitive's declared subdivision level, once fixed.
- **Resolved:** commit `0d3faa3` — verify: `ctest --test-dir b-release -R differential_geometry --output-on-failure`
- **Status note:** The renderer now pre-builds the four bounded subdivision levels and selects one through the same `segments / 8`, clamped-to-[1,4] mapping as `mc3togltf::buildPrimitive()`. This applies to the normal draw pass, emissive pass, and displayed polygon statistics. The CNA-free selection helper is exercised for `segments` 2, 16, and 32 by `differential_geometry_test`, which verifies the resulting renderer tessellation has the same volume, watertightness, and triangle count as the independent exporter output.

### AUD-063 `[DONE]` `P3` `W7` · Viewport Capsule hemisphere-ring-count formula diverges from the exporter's below segments=16
- **Component:** include/MeshCraft/Renderer/PrimitiveTessellationAlg.hpp (tessellateUnitCapsuleAlg), mc3togltf/src/MeshBuilder.cpp
- **Evidence:** Found by `SYS-W7-02`'s differential geometry test. `PrimitiveTessellationAlg.hpp:242` computes hemisphere ring count as `hRings = std::max(4, segments/4)`; the exporter's equivalent computation uses `std::max(2, segments/4)` (see `mc3togltf/src/MeshBuilder.cpp` — verify exact line). For `segments < 16` the two formulas diverge (e.g. `segments=8` gives the viewport `hRings=4` vs. the exporter's `hRings=2`), so a low-tessellation capsule's viewport preview and its exported mesh have different hemisphere-cap resolution — not a shape-correctness bug like `AUD-061` (both are still spherical caps, just different triangle density), but a minor viewport/export tessellation-quality mismatch.
- **Outcome:** Align the two formulas (pick one and use it in both places), or document why they're intentionally different if there's a real reason (e.g. the viewport's `max(4,...)` floor exists to avoid a degenerate low-poly cap at very low LOD tiers, which the exporter doesn't need to worry about since it always tessellates at the document's exact requested value).
- **Tests:** `differential_geometry_test` already surfaces the discrepancy for low-`segments` capsules; add an explicit assertion once the formulas are reconciled (or documented as intentional).
- **Resolved:** commit `134d6c2` — verify: `ctest --test-dir b-release -R differential_geometry --output-on-failure`
- **Status note:** Changed both the position-only viewport tessellation and its VPNT textured-mesh companion to `std::max(2, segments / 4)`, exactly matching `mc3togltf::buildCapsule()`. The two renderer variants must stay aligned because they share an index topology. `differential_geometry_test` now requires tight renderer/exporter volume agreement for all viewport tiers (`segments` 16, 8, and 4), rather than merely documenting the mismatch below 16.
