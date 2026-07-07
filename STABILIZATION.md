# MeshCraft Stabilization Policy and Summary

_Last updated: 2026-07-07, re-verified as part of a conservative-maintainer audit — every count below was recomputed directly from `plan.md`'s per-row status markers and `ctest`, not carried over from a prior revision of this document._

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

The full backlog is **650** `STAB-XXXX` tasks in `plan.md`, sectioned S0-S20. Each gate below requires its **entire** `STAB-XXXX` range green — not just the handful of tasks in that gate's "Priority Execution Order" shortlist (`plan.md`'s own fast-path subset for getting a gate's headline risk closed quickly). Counts below are per-row status markers within each gate's exact ID range, recomputed directly from `plan.md` on 2026-07-07.

Overall: **620 ✅ / 29 🟡 / 0 🧪 / 1 📋 / 0 🔴** across all 650 rows.

| Gate | Name | Required range | Status |
|------|------|-----------------|----------------------|
| **Gate 0** | Build | STAB-0001–0025 (25 rows) | 24 ✅ / 1 🟡 — the 1 remaining row (STAB-0012, MinGW cross-compile) is blocked on a CNA-side gap out of this project's scope, not unattempted |
| **Gate 1** | Format | STAB-0066–0150 (85 rows) | 84 ✅ / 1 🟡 |
| **Gate 2** | Export | STAB-0151–0260 (110 rows) | 110 ✅ / 0 🟡 — **fully green** |
| **Gate 3** | Editor safety | STAB-0261–0335 (75 rows) | 73 ✅ / 2 🟡 |
| **Gate 4** | Registry/AI | STAB-0336–0410 (75 rows) | 73 ✅ / 2 🟡 |
| **Gate 5** | Large scene | STAB-0411–0470 (60 rows) | 57 ✅ / 3 🟡 |
| **Gate 6** | Documentation | STAB-0576–0650 (75 rows) | 73 ✅ / 1 🟡 / 1 📋 — STAB-0617 (large-file audit, needs a live display to verify after a mechanical split) and STAB-0650 (CI report, blocked on the repo owner rotating a PAT before CI can even run) are the only 2 not done |

No gate is 100% ✅ in the strictest sense except **Gate 2 (Export)**, which is fully green. Every remaining row across every other gate is a documented, permanently-flagged 🟡 (genuinely needs a live interactive display session, is blocked on the CNA repo which this project may not modify, or is a deliberate product-scope decision awaiting a call from the project owner) or the single 📋 blocked on external action (STAB-0650). **None are "unattempted" or "unverified claimed as done."** See `plan.md`'s own per-section summary table (bottom of the file) for the exact section-by-section breakdown, and its "Post-650 Follow-Up Findings" section for bugs found and fixed after the original 650-row backlog was substantially closed. `NEXT.md` tracks the specific next task to pick up, if any (as of this writing, there is no open stabilization-plan backlog left — see `NEXT.md` §1).

---

## Current Test Suite (2026-07-07)

**66 CTest tests, all passing** — verified from a genuinely clean build (`rm -rf cmake-build-debug`, full reconfigure + rebuild, then `ctest`) as part of this session's audit, not carried over from an incrementally-updated build directory. Label breakdown: `ai` 1, `commands` 1, `export` 44, `format` 3, `registry` 1, `render` 16. Full per-test reference (all 66, not just a representative subset) lives in `TESTING.md` — this section is intentionally not a duplicate list; see there.

Also verified independently of the root build: standalone (CNA-free) configure/build/test for `mc3/` (1/1), `mcb/` (1/1), `mc3togltf/` (41/41), `mc3tomcb/` (3/3) — each must stay buildable without CNA/ImGui, per `CLAUDE.md`. Re-run from scratch 2026-07-07, not assumed from a prior count.

Run: `cd cmake-build-debug && ctest --output-on-failure` (see `NEXT.md` section 7 for the full command list, including a fresh configure).

---

## Known Gaps (re-verified 2026-07-07 — headlines below are still accurate; no new architectural gaps found this session beyond what's listed)

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
