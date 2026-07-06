# MeshCraft Stabilization Policy and Summary

_Last updated: 2026-07-06_

---

## Current Stabilization Policy

1. **No new features until stabilization gates are green.**
2. Every task in `plan.md` requires:
   - Acceptance criteria
   - Verification command or test
   - Status changed to ✅ only after a test exists, is registered, runs, and passes
3. Status symbols: ✅ verified · 🟡 partial · 🧪 has a plan/test but not executed or not fully verified · 📋 planned · 🔴 blocked
4. Claude Code must ask before implementing any `plan.md` task (per `CLAUDE.md`'s workflow).
5. No CNA changes without owner permission.
6. No `${meta-gl_SOURCE_DIR}/include` in `CMakeLists.txt`.
7. No `Mc3Document` public API changes without checking `mc3togltf`, `mc3tomcb`, and all test XMLs.

---

## Stabilization Gates

The full backlog is 650 `STAB-XXXX` tasks in `plan.md`, sectioned S0-S20. Each gate below requires its **entire** `STAB-XXXX` range green — not just the handful of tasks in that gate's "Priority Execution Order" shortlist (`plan.md`'s own fast-path subset for getting a gate's headline risk closed quickly).

| Gate | Name | Required range | Priority-list status |
|------|------|-----------------|----------------------|
| **Gate 0** | Build | STAB-0001–0025 green | ✅ done |
| **Gate 1** | Format | STAB-0066–0150 green | ✅ done |
| **Gate 2** | Export | STAB-0151–0260 green | ✅ done |
| **Gate 3** | Editor safety | STAB-0261–0335 green | ✅ P1 items done; P2/P3 remain |
| **Gate 4** | Registry/AI | STAB-0336–0410 green | ✅ priority-list items done (S9 registry edge cases, S10 AI mock tests + XSD validation); P2/P3 remain |
| **Gate 5** | Large scene | STAB-0411–0470 green (subset) | ✅ priority-list items done (S6 mesh-reuse check + 500-object test); P2/P3 remain |
| **Gate 6** | Documentation | STAB-0576–0650 green | ✅ 71/75 done; the remaining 4 are all genuinely blocked, not unattempted — STAB-0617 (ImGui refactor needs a live display to verify after splitting), STAB-0642 (needs Blender), STAB-0643 (needs a live browser), STAB-0650 (needs CI actually running, blocked on git remote credentials) |

None of the gates are fully green yet in the strictest sense (a handful of P2/P3 rows remain in each), but every remaining row across every gate is either lower-priority or genuinely blocked on a missing tool/live session documented in `NEXT.md` §5 — not unattempted. See `plan.md`'s per-section summary table for the exact `STAB-XXXX` counts remaining in each. `NEXT.md` tracks the specific next task to pick up.

---

## Current Test Suite (2026-07-03)

20 CTest tests, all passing:

| Test | What it covers |
|------|---------------|
| `smoke_test` | Editor binary starts, opens scene, exits cleanly |
| `xsd_validation` | All `test/*.mc3.xml` validate against `mc3/mc3.xsd` |
| `mc3_registry` | ModelRegistry SQLite CRUD + migration + edge cases |
| `mc3_ai` | AiAssistant JSON helpers, AI-response validation pipeline (extract/repair/parse/empty-check/XSD-validate), mock-HTTP-server round-trips (success/truncation/HTTP error) |
| `mc3_roundtrip` | Full XML parser/writer roundtrip (all features, including N1-N7) |
| `mc3_commands` | Editor command algorithms, undo/redo, save/load workflows, keybinding/prefs/macro persistence, hierarchy filtering |
| `mcb_roundtrip` | MCB binary encode/decode roundtrip (N1-N7 included) |
| `mc3tomcb_roundtrip` | `mc3tomcb` CLI roundtrip (`mc3.xml` ↔ `.mcb`) |
| `mc3togltf_gltf` | Animation + house GLB export + magic bytes |
| `mc3togltf_all_primitives` | All primitive types export without error |
| `mc3togltf_export_verification` | Node count, mesh presence, material names |
| `mc3togltf_large_scene` | Static 200-object scene export |
| `mc3togltf_csg_strict` | CSG fails hard without the approximate-export flag |
| `mc3togltf_csg_export` | Union/difference/intersection real export |
| `mc3togltf_csg_unsupported` | Unsupported CSG child type detected |
| `mc3togltf_csg_nested` | Nested CSG operations export correctly |
| `mc3togltf_instance_deform_cache` | Deform cache key produces separate meshes |
| `mc3togltf_float_cache_key` | Close float dimensions don't collide |
| `mc3togltf_large_scene_generated` | Python-generated 200-object test; asserts mesh reuse (not 1 mesh per node) |
| `mc3togltf_large_scene_500` | Same generator scaled to 500 objects; asserts export completes in < 30s |

Also verified independently of the root build: standalone (CNA-free) configure/build/test for `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` — each must stay buildable without CNA/ImGui, per `CLAUDE.md`.

Run: `cd cmake-build-debug && ctest --output-on-failure` (see `NEXT.md` section 7 for the full command list, including a fresh configure).

---

## Known Gaps (as of 2026-07-03)

See `NEXT.md` section 5 ("Known bugs and limitations") for the authoritative, actively-maintained list — it is kept current every session, unlike this document's historical narrative below. Headlines:

- CI workflow exists but is parked deactivated under `.github_/` — the git PAT lacks the `workflow` OAuth scope needed to activate it (owner action required).
- SVG texture rasterization (N1) is parsed/serialized but not rasterized (stub only).
- Embedded glTF (N2) is parsed/serialized but not resolved/inlined by `GltfExporter`.
- `EditorViewport` is not integrated into `MeshCraftApplication`'s render loop.
- N3-N7 extensions (scripts, sounds, music, triggers, scene states) are fully round-tripped (XML, MCB, XSD) but **not executed/applied at runtime** — no Lua interpreter, no audio playback, no trigger-firing event system, no "switch active scene state" logic. This is by design at the current stage (data model first), not a bug — see `MC3_FORMAT.md` for the per-feature status notes.
- MCB compression flag is reserved in the header but not implemented.

---

## Historical Note

Sections S1-S12 (referenced in commit history and early planning docs) predate the current S0-S20 / `STAB-XXXX` structure and were folded into it during the 2026-06-27 replan. All N1-N7 schema extensions (SVG texture, embedded glTF, Lua scripts, sounds/music, triggers, scene states, meta map) were completed — data model, parser, writer, MCB support, and XSD fixtures — before that replan and are tracked as done in `plan.md`.

---

## Where to Look Next

- **`plan.md`** — the full 650-task backlog (STAB-0001 through STAB-0650), the authoritative per-task status.
- **`NEXT.md`** — short, operational: current status, current blocker (if any), and the exact next task to pick up.
- **`MC3_FORMAT.md`** — the format specification, including per-feature implementation status notes.
