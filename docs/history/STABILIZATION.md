# MeshCraft Stabilization Policy and Summary

_Last updated: 2026-07-18, re-verified against current `plan.md`/`ctest` state (was last touched 2026-07-07; every count below was recomputed fresh, not carried over)._

**Gate status as of this update: all gates (A–E) satisfied.** `plan.md`
has zero open P0 findings — the only remaining `AUD-###` rows are P1–P3,
each owner-gated/environment-blocked (CI parked, no Android NDK) or
DEFERRED (informational, by-design). Gate F ("no new features until
gates are green") was explicitly, knowingly overridden by the user for a
batch of `SYS-W14-##` feature rows — a granted exception, not a broken
gate; see `NEXT.md` §9 for how that's actually been handled in practice.

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

This file defines the **stable quality bar**. Live task status and counts belong
in `plan.md` (the single active backlog) and `NEXT.md` (the current baseline) —
they are deliberately kept out of this file so it does not go stale. The gates
below are exit criteria, not a progress tracker.

| Gate | Name | Exit criterion |
|------|------|-----------------|
| **A** | No known P0 defects | No open UB / crash / data-loss / unsafe-input / broken round-trip / use-after-free / race / silent-destructive-fallback / invalid-output-reported-as-success. |
| **B** | Tests validate production code | Tests exercise the real production path, not a copied `*Alg` helper that production doesn't call. |
| **C** | Truthful capability claims | Every backend / platform / format feature / UI action is documented as exactly one of: implemented+verified, implemented+unverified, partial (with stated limits), or unsupported+rejected. No selectable-but-non-functional configs. |
| **D** | Input budgets & deterministic diagnostics | Every externally-controlled parser/importer has finite-number checks, size/count/depth limits, allocation budgets, clear diagnostics, deterministic failure, and pathological-input tests. |
| **E** | No documentation drift | XSD / parser / writer / model / MCB / exporter / UI / examples / docs checked for parity by automated matrices where possible. |
| **F** | New-feature readiness | Major new features start only after P0 tasks are closed, baseline tests are green in available environments, and the relevant architecture is not duplicated/unsafe. Small safety/diagnostics/recovery features may land earlier. |

Legacy note: earlier revisions of this file tracked a 650/723-row `STAB-XXXX`
gate scheme (Gate 0–6). That backlog is archived in
`docs/history/plan_stabilization_master.md`; its status is superseded by
`plan.md`.

---

## Current Test Suite (2026-07-18)

**132 CTest tests, all passing** — re-verified against a fresh build as of today's last commit (was 66 at the 2026-07-07 count above; growth is organic backlog/feature work across many sessions since, not a one-time jump — see `plan.md`/`NEXT.md` for the session-by-session history). Full per-test reference lives in `TESTING.md` — this section is intentionally not a duplicate list; see there, and re-run `ctest -N` for the live count rather than trusting any number here as time passes.

Also verified independently of the root build: standalone (CNA-free) configure/build/test for `mc3/`, `mcb/`, `mc3togltf/`, `mc3tomcb/` — each must stay buildable without CNA/ImGui, per `CLAUDE.md`.

Run: `cd b-release && ctest --output-on-failure` (see `NEXT.md` section 7 for the full command list, including a fresh configure).

---

## Known Gaps (re-verified 2026-07-18 — see `NEXT.md` §5 for anything newer)

See `NEXT.md` section 5 ("Known bugs and limitations") for the authoritative, actively-maintained list — it is kept current every session, unlike this document's historical narrative below. Headlines:

- CI workflow exists but is parked deactivated under `.github_/` — the git PAT lacks the `workflow` OAuth scope needed to activate it (owner action required).
- SVG texture rasterization (N1) is parsed/serialized but not rasterized (stub only).
- Embedded glTF (N2) is parsed/serialized but not resolved/inlined by `GltfExporter`.
- `EditorViewport` — **resolved 2026-07-17**: deleted outright (abandoned scaffolding, never wired in), not integrated.
- N3-N7 extensions (scripts, sounds, music, triggers, scene states) are fully round-tripped (XML, MCB, XSD) but **not executed/applied at runtime** — no Lua interpreter, no trigger-firing event system, no "switch active scene state" logic. The editor does have a sound/music *preview* button (`Editor::AudioPreview`, added 2026-07-18) — that's an editing convenience, not a runtime audio engine for the exported scene. By design at the current stage (data model first), not a bug — see `MC3_FORMAT.md` for the per-feature status notes.
- MCB compression flag is reserved in the header but not implemented.

---

## Historical Note

Sections S1-S12 (referenced in commit history and early planning docs) predate the current S0-S20 / `STAB-XXXX` structure and were folded into it during the 2026-06-27 replan. All N1-N7 schema extensions (SVG texture, embedded glTF, Lua scripts, sounds/music, triggers, scene states, meta map) were completed — data model, parser, writer, MCB support, and XSD fixtures — before that replan and are tracked as done in `plan.md`.

---

## Where to Look Next

- **`plan.md`** — the single active backlog (`AUD-###` audit findings + `SYS-###` workstream tasks), the authoritative per-task status.
- **`NEXT.md`** — short, operational: current status, current blocker (if any), and the exact next task to pick up.
- **`MC3_FORMAT.md`** — the format specification, including per-feature implementation status notes.
