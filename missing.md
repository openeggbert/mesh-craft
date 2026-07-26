# MeshCraft Editor — mc3 Format Coverage Gaps

Analysis date: 2026-07-19 (refreshed from the 2026-07-18 analysis — see
"Resolved since 2026-07-18" below for what closed same-day, after this
file's own last edit; "Resolved since 2026-07-10" further down for the
earlier round). Scope:
what the **MeshCraft editor's interactive GUI** can create/edit/view, versus
what the **mc3 format** (mc3.xsd, `Mc3Document` and friends) actually
supports. This is *not* about `mc3togltf` export fidelity — that surface is
covered by `plan.md`'s `AUD-###`/`SYS-W7-##` rows. This is specifically about
editor UI gaps: format features that parse/round-trip correctly but have no
interactive editing surface, or have one that's incomplete.

Findings are grouped by severity: **still-open total gaps** (zero UI,
XML-hand-edit only) first, then **still-open partial gaps** (UI exists but
incomplete or buggy), then **non-gaps** worth noting for context, then two
resolved-history lists (**2026-07-18**, then the earlier **2026-07-10**
round) for historical credit, then a summary table. As of this refresh,
sections 1 and 2 below have no remaining open findings — everything that
was open as of 2026-07-18 closed the same day.

---

## Resolved since 2026-07-18

All 4 "total gap" findings (N8/N9/N10/N11), plus the texture file-browse
gap and the `coordinate_system`/Area partial gaps, closed the same day as
this file's own last edit (`SYS-W14-10` through `SYS-W14-17`,
`git log --oneline --grep="SYS-W14-1"`):

- **N8 Object `scriptId` attachment** — a "Script" combo added to
  `PropertiesPanel.cpp`, mirroring the existing Material combo's
  structure (none-sentinel, mixed-selection handling, pushUndo/
  markModified on change) (`SYS-W14-10`, `643b238`).
- **N9 Library metadata / imports (`.mc3lib`)** — new "Library (.mc3lib)"
  section (namespace/version + a hash-recompute button) and a new
  "Imports" tab (an index-based row editor for the ordered `doc.imports`
  vector) in the Scene Properties panel (`SYS-W14-13`, `8a2d02c`).
- **N10 Semantic JSON file I/O (`.mc3.json`)** — Open/Save/Save As now
  dispatch on extension (`.mcb`/`.json`/else) via one shared
  `loadSceneFileDispatched()`, replacing 3 independently-drifted copies
  of that dispatch; also fixed a related pre-existing bug where `Save`
  always wrote XML regardless of the file's real extension (`SYS-W14-11`,
  `166a712`).
- **N11 Asset metadata (`assetMetadata`)** — all 23 fields, in a
  collapsible "Asset Metadata" section on the Defs tab (definitions, not
  placed instances — matches the field's own "present only on
  definitions" documented scope) (`SYS-W14-12`, `0634aa8`).
- **Texture file-browse dialog** — a "..." Browse button next to each of
  the 5 material texture-slot fields, using CNA's own `FileDialog`
  device (previously fully implemented but never called from this repo)
  (`SYS-W14-15`, `7229b63`).
- **`coordinate_system`** — fully implemented (`SYS-W14-14`, 2026-07-26).
  A shared CNA-free helper drives Z-up ↔ Y-up conversion for rendering,
  picking, gizmos, walk collision, cameras/lights and glTF export. Existing
  Z-up documents can be explicitly normalized to Y-up from Scene Properties;
  there is no silent load-time rewrite.
- **Area properties panel** — investigated whether Area objects carry
  distinct data beyond size, or whether `doc.triggers` links back to a
  specific object; confirmed neither (no `ObjectType::Area` parser case
  exists — Area is just a Box-shaped primitive with a different type
  tag; `Mc3Trigger` has no field referencing an `Mc3Object` at all).
  **Confirmed the existing labeled-generic-Box editor is the complete,
  correct UI given the current data model**, not a partial one — no code
  change needed (`SYS-W14-17`, `784a222`).
- **Undo/redo coverage** — a fresh independent 2-agent audit found and
  fixed 27 further real gaps (6 missing `pushUndo()` entirely, 21 more
  instances of the `AUD-036`-style nested-`IsItemActivated()` dead
  pattern) across `PropertiesPanel.cpp`/`MeshCraftApplication_UiLeftPanel.cpp`
  (`SYS-W14-16`, `4c2d535`). **Still accurately a manual discipline, not a
  structural guarantee** — explicitly confirmed intentional scope, not an
  oversight, when `SYS-W9-01` was closed the same day: "no formal
  transaction-abstraction class was built; the manual
  pushUndo()-before-every-mutation discipline remains, now backed by
  three audit rounds rather than a structural guarantee" (`dc69b7a`).

## Resolved since 2026-07-10

All 7 of the original "zero UI" extension-namespace findings, plus 6 of 12
"partial gap" findings, were closed in a follow-up session
(`STAB-0703`..`STAB-0721`, `git log --oneline --grep="STAB-070"` /
`--grep="STAB-071"`) that this file's original analysis predates:

- **N1 SVG textures** — full add/remove/edit UI, External-vs-Inline toggle
  (`STAB-0703`, `MeshCraftApplication_UiLeftPanel.cpp`'s "Tex" tab).
- **N2 Embedded glTF** — full add/remove UI for `doc.embeds`, External(.glb
  path)-vs-Inline(base64) toggle, shows the `embed:<id>` reference string to
  paste into a Mesh's Source field (`STAB-0704`, same file's "Embeds" tab).
- **N3 Scripts** — full add/remove/edit UI (id/type/plain-text source) for
  `doc.scripts` itself (`STAB-0705`, "Scripts" tab) — **but see the new N8
  finding below: the object-level `scriptId` field that *attaches* a script
  to an object still has no UI.**
- **N4 Sounds and Music** — full add/remove UI for both, with playback
  preview (▶/■ buttons wired to real `SoundEffect`/`SoundEffectInstance`
  playback) — goes beyond what the original finding expected (`STAB-0706`,
  "Audio" tab).
- **N5 Triggers** — full add/remove UI with a per-trigger step editor (type
  combo + ref-id field) (`STAB-0707`, "Triggers" tab).
- **N6 Scene States** — full add/remove UI with per-state object-override
  editing (independent visible/position/rotation/material override toggles)
  (`STAB-0708`, "States" tab).
- **N7 Meta** — key/value editor (rename/edit/remove/add) for `doc.meta`
  (`STAB-0709`, `PropertiesPanel.cpp`). Note: `doc.metadata` — a separate,
  legacy pass-through map — is explicitly still out of scope per that
  commit's own comment.
- **IcoSphere subdivision level** — now a live slider (1-4) instead of a
  hardcoded label (`STAB-0711`).
- **Primitive `axis` field** (Cylinder/Plane/Disk/Capsule) — shared
  `drawAxisCombo()` UI added; Cone intentionally excluded since its mesh
  generation is Y-axis-only (`STAB-0712`).
- **Animation `autoplay`** — checkbox added next to the existing `loop`
  checkbox (`STAB-0714`).
- **New Deform/Material animation channels seeding hardcoded 0.0** — now
  read the object's actual current value via `resolveObjectPropertyValueAlg`
  (`STAB-0715`).
- **Camera orthographic single-scalar model** — `Mc3Camera::orthoAspect`
  added end-to-end (XSD, XML/JSON parse+write, exporter's `xmag`/`ymag`,
  and a UI drag-float), so `orthoSize`+`orthoAspect` can represent any
  xmag/ymag pair (`STAB-0695`, closed).
- **Whole-scene Import menu** — File → Import OBJ... adds a new top-level
  object from an OBJ file (`STAB-0717`).
- **Non-glTF export** — File → Export OBJ... added alongside the existing
  GLB/Selection/Template export actions (`STAB-0718`).
- **Group missing from Add menu** — "Group" menu item added alongside
  Box/Sphere/.../CSG in the Add menu.

## 1. Total gaps — zero editor UI

None remaining — N8/N9/N10/N11 (the 4 findings originally in this
section) were all closed 2026-07-18; see "Resolved since 2026-07-18"
above.

## 2. Total gaps — document-level attributes

- **`rotation_units` / `euler_order`**: still no *editing* UI anywhere (no
  combo/input field — confirmed no change since 2026-07-10), and rendering
  still hardcodes degrees/XYZ regardless of the document's declared
  convention. **Upgraded from "zero UI anywhere" to "read-only notice
  added"**: `checkRotationConventionNotice()` (`STAB-0701`,
  `MeshCraftApplication_FileOps.cpp`) now shows a 6-second status-bar toast
  on every load path naming the file's declared convention and stating the
  editor's live preview doesn't honor it. **Confirmed still won't-fix by
  design** — `NEXT.md` explicitly lists this as "Confirmed, by design,
  deferred ... won't-fix, tracked as `STAB-0701`"; no open task adds actual
  editing UI for these fields, and a related item (`STAB-0710`) was closed
  as won't-fix "superseded by STAB-0701's decision."

## 3. Partial gaps — UI exists but incomplete, inconsistent, or buggy

None remaining as open code gaps — `coordinate_system`, the texture
file-browse dialog, undo/redo coverage, and Area's properties panel (the
4 findings originally in this section) were all resolved or formally
closed 2026-07-18; see "Resolved since 2026-07-18" above for exactly what
changed and what's still intentionally not a structural guarantee
(undo/redo) or not read anywhere (`coordinate_system`, by design).

## 4. Non-gaps (re-confirmed 2026-07-18, no regressions)

- **Materials**: still essentially complete — all scalar fields and all 5
  texture slots editable.
- **CSG**: still fully supported — type-switching, cutter-role toggling,
  nesting.
- **Animation interpolation**: still all 3 modes (Step/Linear/CubicBezier)
  selectable per-keyframe.
- **Lights, Cameras, Extrude, Groups/Definitions/Instances,
  `model`/`unit`/`default_camera`**: not re-verified this pass (no signal
  they changed); treat as still complete per the 2026-07-10 analysis unless
  contradicted by something you find.

---

## Summary table

| Area | Status |
|------|--------|
| N1-N7 (SVG textures, embeds, scripts, audio, triggers, scene states, meta) | ✅ resolved (`STAB-0703`..`0709`) |
| N8 Object `scriptId` attachment | ✅ resolved (`SYS-W14-10`) |
| N9 Library metadata / imports (`.mc3lib`) | ✅ resolved (`SYS-W14-13`) |
| N10 Semantic JSON file I/O (`.mc3.json`) | ✅ resolved (`SYS-W14-11`) |
| N11 Asset metadata (`assetMetadata`) | ✅ resolved (`SYS-W14-12`) |
| `rotation_units`/`euler_order` | 🟡 read-only load-time notice only; no editing UI; won't-fix by design (`STAB-0701`) |
| IcoSphere subdivision level | ✅ resolved (`STAB-0711`) |
| Primitive `axis` | ✅ resolved (`STAB-0712`) |
| `coordinate_system` | ✅ right-handed Y-up and Z-up honored in the editor and glTF export; explicit Normalize to Y-up command (`SYS-W14-14`) |
| Animation `autoplay` | ✅ resolved (`STAB-0714`) |
| Animation Deform/Material channel initial value | ✅ resolved (`STAB-0715`) |
| Camera orthographic `xmag`/`ymag` | ✅ resolved (`STAB-0695`) |
| Texture import (file-browse dialog) | ✅ resolved (`SYS-W14-15`) |
| Whole-scene Import (OBJ) | ✅ resolved (`STAB-0717`) |
| Non-glTF Export | ✅ resolved (`STAB-0718`, OBJ export) |
| Undo/redo structural guarantee | 🟡 27 further gaps closed (`SYS-W14-16`); still intentionally a manual discipline, not structural (confirmed scope, not an oversight — see `dc69b7a`) |
| Group in Add menu | ✅ resolved |
| Area properties panel | ✅ confirmed complete given the current data model, no code change needed (`SYS-W14-17`) |
| Materials, Lights, Cameras (core), Extrude, CSG, Groups/Definitions/Instances, Interpolation, model/unit/default_camera | ✅ complete |
