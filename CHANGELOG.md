# Changelog

This project has not tagged a release yet — everything below describes
`develop` branch history. Reconstructed from `git log`, not hand-maintained
during development, so treat it as a summary of *what happened*, not a
promise of exact chronological order within each entry.

## [Unreleased] — 0.1.0

Currently in an active **stabilization phase**: fixing and test-covering
what already exists rather than adding features. See `STABILIZATION.md`
for the policy and `plan.md` for the full 650-task backlog
(`STAB-0001`–`STAB-0650`, sectioned S0–S20).

**Note on this section's title:** `plan.md`'s own verification text for
this file asks for "completed feature groups (A–N), stabilization groups
(S1–S12)" — that's the *original* pre-replan terminology. It doesn't map
onto anything findable in this repo's actual history (git log only shows
lettered feature groups `E` through `T`, not `A`–`D`, and the
stabilization sections were renumbered `S0`–`S20` during a 2026-06-27
replan — see `STABILIZATION.md`'s "Historical Note"). This changelog
uses what's actually verifiable in `git log` instead of reviving stale
labels.

As of this writing: **194 ✅ done, 3 🟡 partial, 136 🧪 has a plan but
not executed, 317 📋 not started** out of 650 `STAB-XXXX` tasks (see
`plan.md`'s summary table for the exact per-section breakdown, or
`NEXT.md` for the current session-to-session status).

Highlights from the stabilization phase so far:
- Full XML round-trip test coverage for all 10 primitive types and all
  N1–N7 schema extensions, including edge cases.
- MCB binary format round-trip test coverage (base scene + all N1–N7
  extension types).
- XSD validation fixtures for every schema section, including a
  from-scratch audit that found and fixed **8 real gaps** between
  `mc3.xsd` and what `Mc3XmlWriter.cpp` actually writes (object `id`,
  `layer`, per-object `<state>`, instance `material_override`/
  `variants`, `uv_mapping`'s real attributes, environment
  `background_texture`/`skybox_texture`, texture `name`) — plus one
  genuine round-trip bug found along the way (texture rename was
  silently discarded on save/reload).
- Mock-HTTP-server test coverage for the AI Assistant integration (no
  real network call needed) and real XSD validation of AI-generated
  scene XML before it can be applied.
- Editor undo/redo, save/load, auto-save, and dialog-lifecycle test
  coverage across the whole command surface.
- A security/robustness pass (S19): confirmed the API key is never
  persisted to disk or logged, SQLite queries are fully parameterized,
  and the parser handles hostile inputs (path traversal, a 10MB
  malformed-XML file, a 10MB texture URI) without crashing or hanging.
- `--version` CLI flag and a project version number (`0.1.0`).

## Major features (pre-stabilization)

Chronological by when each landed, reconstructed from `git log`'s
lettered commit-message convention:

- **M1 — `<include file="…"/>`**: share definitions/materials/textures
  across scenes without inlining them.
- **M2 — Model Registry**: SQLite-backed library of reusable model
  definitions, browsable and insertable from the editor.
- **M3 — AI Assistant**: Claude API integration — describe a change in
  natural language, review the generated scene XML, apply it or save
  individual definitions to the registry.
- **N1–N7 — schema extensions**: SVG textures, embedded glTF, Lua
  scripts, sound/music, triggers, scene states, and a general-purpose
  meta key/value map. All fully round-tripped (XML, MCB, XSD); N3–N7
  are data-only so far — nothing in the editor executes them yet (no
  Lua interpreter, no audio playback, no trigger-firing, no
  state-switching — see `MC3_FORMAT.md`'s per-section status notes).

## Earlier feature work

Smaller, more numerous changes grouped by the same lettered convention.
Only groups that actually appear in `git log` are listed — several
letters that would sit between these (e.g. `A`–`D`) either predate this
repository's history or were never used as a label.

- **E — editor UX**: named layers (hierarchy filter + Properties
  field), "Export Subtree as Template".
- **F — file/scene operations**: mesh-source browse dialog, Export
  Selection, Merge Scene, configurable auto-save interval, rotating
  `.backup.1`/`.backup.2` backups, headless screenshot mode, an Export
  Settings dialog (GLB/GLTF toggle).
- **G — geometry**: procedural LOD (3 segment tiers by camera
  distance) for round primitives.
- **H — advanced editing tools**: proportional editing (Gaussian
  falloff on nearby objects during Move), Scatter Along Curve.
- **R — rendering**: 3-point directional lighting for the solid
  primitive draw pass.
- **S (early) — mc3togltf stabilization**: canonicalized the mesh
  `src` attribute, fixed plane `size` (vec3 → canonical vec2), removed
  a duplicate XML parser from `mc3togltf` in favor of the shared `Mc3`
  library, added `mc3togltf` to the top-level CMake build, added XSD
  validation, warnings for unimplemented primitives/unsupported CSG
  nodes, rewrote `README.md`, created `MC3_FORMAT.md`. (This is a
  *different* numbering than the current `S0`–`S20` stabilization
  sections in `plan.md` — see the note above.)
- **T — tooling/integration**: replaced a `mc3togltf` subprocess call
  with a direct library call from the editor.

## Where to look for more detail

- `plan.md` — the authoritative per-task status for the current
  stabilization effort.
- `NEXT.md` — short, operational: current status, current blocker (if
  any), exact next task.
- `git log` — the actual commit history; this file summarizes it, it
  doesn't replace it.
