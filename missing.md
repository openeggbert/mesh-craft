# MeshCraft Editor — mc3 Format Coverage Gaps

Analysis date: 2026-07-18 (updated from the original 2026-07-10 analysis —
see "Resolved since 2026-07-10" below for what closed in between). Scope:
what the **MeshCraft editor's interactive GUI** can create/edit/view, versus
what the **mc3 format** (mc3.xsd, `Mc3Document` and friends) actually
supports. This is *not* about `mc3togltf` export fidelity — that surface is
covered by `plan.md`'s `AUD-###`/`SYS-W7-##` rows. This is specifically about
editor UI gaps: format features that parse/round-trip correctly but have no
interactive editing surface, or have one that's incomplete.

Findings are grouped by severity: **still-open total gaps** (zero UI,
XML-hand-edit only) first, then **still-open partial gaps** (UI exists but
incomplete or buggy), then **non-gaps** worth noting for context, then a
**resolved-since-2026-07-10** list for historical credit, then a summary
table.

---

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

- **N8 — Object-level script attachment** (`Mc3Object::scriptId`,
  `mc3/include/MeshCraft/Mc3/Mc3Object.hpp`). Scripts themselves are fully
  editable (`doc.scripts`, N3 above), but there is no UI anywhere to set an
  *object's* `scriptId` to attach one of those scripts to it — confirmed via
  `grep -rn "scriptId" src/MeshCraft/` (zero hits). XML-hand-edit only.
- **N9 — Library metadata and imports** (`Mc3Document::library`
  (`Mc3LibraryInfo`), `Mc3Document::imports` (`Mc3Import`), the `.mc3lib`
  reusable-library file format). Zero editor UI — no panel shows/edits the
  library namespace/version, no way to add/remove an `<imports>` entry, and
  no File-menu action creates/opens a `.mc3lib` file (`saveToLibraryFile`/
  `saveToLibraryJsonFile` exist in `mc3/src/Mc3Document.cpp` but nothing in
  `src/MeshCraft/` calls them). This is a newer, still-evolving format
  addition (part of a separate cross-repo initiative — see the
  `project_meshworld_r_series_cross_repo` memory) — may not be worth
  editor UI yet if that initiative itself isn't finished; flagged for a
  human call, not assumed.
- **N10 — Semantic JSON file I/O** (`.mc3.json`, `Mc3JsonWriter`/
  `Mc3JsonParser`). `Mc3Document::loadFromJsonFile`/`saveToJsonFile` exist
  and work, but the editor's Open/Save/Save As (`MeshCraftApplication_FileOps.cpp`)
  exclusively call the XML load/save path — no File-menu entry or dialog
  filter offers `.mc3.json` at all. XML-only in practice from the GUI.
- **N11 — Asset metadata** (`Mc3Object::assetMetadata`, `Mc3AssetMetadata`
  struct — category/subcategory/tags/bounds/facing/sockets/materialSlots/
  collisionProxy/lods/license/provenance/semanticVersion). Zero editor UI —
  confirmed via `grep -rn "assetMetadata\|AssetMetadata" src/MeshCraft/`
  (zero hits). A large struct; a full editor would be a substantial UI
  task, not a quick add.

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

- **`coordinate_system` is write-only.** The invalid `"left_handed_y_up"`
  combo option was removed (`STAB-0713` — the combo now only offers the 2
  XSD-valid values), but the deeper half of the original finding persists
  unchanged: `coordinateSystem` is stored and editable, but still **never
  read** anywhere in `mc3togltf/` or `src/MeshCraft/` (confirmed by direct
  grep, no read sites found) — it has zero effect on rendering or export
  today. This is arguably more of a real completeness/correctness gap than
  a pure "missing UI" one: the field can be set correctly through the GUI
  and it still does nothing.
- **No native texture file-browse dialog.** Still true, unchanged — plain
  `ImGui::InputText` path boxes everywhere (material texture slots, the
  Import OBJ dialog), no picker. Notably, the CNA dependency already ships
  a `FileDialog` device (SDL backend) that is never called from anywhere in
  `src/MeshCraft/`/`mc3togltf/` — the capability exists one layer down and
  is simply unused.
- **Undo/redo coverage is a manual discipline, not a structural
  guarantee.** Still true. `SYS-W5-04` (2026-07-17/18) did related
  exhaustive-audit work, but it was scoped to building a lookup cache
  (`Editor::ObjectIndex`), not to converting undo/redo coverage into a
  structural guarantee — its own invalidation-correctness argument still
  rests on "confirmed via grep that `pushUndo()` runs before virtually
  every mutating command" (332 call sites, per a fresh count), the same
  manual-discipline pattern this finding originally flagged, not a
  Command-pattern/wrapper enforcement. Flagged as a risk area, not a
  confirmed live bug.
- **Area objects have no dedicated properties UI.** Upgraded from "generic
  Box editor, cosmetically unclear" to "labeled but still generic":
  `STAB-0721` added an `"Area (trigger zone)"` label when editing an Area
  object, but the editor still falls through to the same generic Box size
  fields immediately below (by design, per that commit's own comment —
  Area objects structurally carry a Box-shaped primitive with no parser
  case of their own, so a fully separate editing block wasn't built).

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
| N8 Object `scriptId` attachment | ❌ no UI |
| N9 Library metadata / imports (`.mc3lib`) | ❌ no UI (newer format, may not need UI yet — human call) |
| N10 Semantic JSON file I/O (`.mc3.json`) | ❌ no UI (File menu is XML-only) |
| N11 Asset metadata (`assetMetadata`) | ❌ no UI (large struct, substantial task) |
| `rotation_units`/`euler_order` | 🟡 read-only load-time notice only; no editing UI; won't-fix by design (`STAB-0701`) |
| IcoSphere subdivision level | ✅ resolved (`STAB-0711`) |
| Primitive `axis` | ✅ resolved (`STAB-0712`) |
| `coordinate_system` | 🟡 UI now XSD-valid, but the value is still never read anywhere (real gap, not just UI) |
| Animation `autoplay` | ✅ resolved (`STAB-0714`) |
| Animation Deform/Material channel initial value | ✅ resolved (`STAB-0715`) |
| Camera orthographic `xmag`/`ymag` | ✅ resolved (`STAB-0695`) |
| Texture import (file-browse dialog) | 🟡 still drag-drop/manual path only |
| Whole-scene Import (OBJ) | ✅ resolved (`STAB-0717`) |
| Non-glTF Export | ✅ resolved (`STAB-0718`, OBJ export) |
| Undo/redo structural guarantee | 🟡 still a manual (grep-audited) discipline, not structural |
| Group in Add menu | ✅ resolved |
| Area properties panel | 🟡 label added, still generic Box editor underneath |
| Materials, Lights, Cameras (core), Extrude, CSG, Groups/Definitions/Instances, Interpolation, model/unit/default_camera | ✅ complete |
