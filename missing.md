# MeshCraft Editor — mc3 Format Coverage Gaps

Analysis date: 2026-07-10. Scope: what the **MeshCraft editor's interactive
GUI** can create/edit/view, versus what the **mc3 format** (mc3.xsd,
`Mc3Document` and friends) actually supports. This is *not* about
`mc3togltf` export fidelity — that surface was already deeply audited this
session (plan.md section S23, 41 findings, 38 fixed). This is specifically
about editor UI gaps: format features that parse/round-trip correctly but
have no interactive editing surface, or have one that's incomplete.

Findings are grouped by severity: **Total gaps** (zero UI, XML-hand-edit
only) first, then **partial gaps** (UI exists but incomplete or buggy),
then **non-gaps** worth noting for context (areas that turned out to be
more complete than expected, or where the real limitation is elsewhere).

---

## 1. Total gaps — zero editor UI (N1-N7 extensions)

All seven mc3 extension namespaces parse and round-trip correctly
(`Mc3XmlParser.cpp`/`Mc3XmlWriter.cpp` both reference every field), and all
export correctly to glTF/GLB where applicable (per the S23 export audit).
But **none of them have any interactive UI in the editor at all** — no
panel, no menu entry, not even a read-only viewer. A user can only create
or modify this data by hand-editing the XML (or writing MCB via the `mc3`
library API directly). Verified by grepping all of `src/MeshCraft/` for
every relevant `Mc3Document` field name and struct type — zero real matches
for any of the seven (two apparent matches were false positives: the
English word "triggers" in unrelated comments).

- **N1 — SVG textures** (`doc.svgTextures`, `Mc3SvgTexture`). The regular
  texture picker/editor only ever touches `doc.textures` (raster textures);
  SVG textures are a fully separate, UI-invisible map.
- **N2 — Embedded glTF** (`doc.embeds`, `Mc3EmbedGltf`, `<mesh src="embed:id"/>`).
- **N3 — Scripts** (`doc.scripts`, `Mc3Script`).
- **N4 — Sounds and Music** (`doc.sounds`, `doc.musicTracks`, `Mc3Sound`/`Mc3Music`).
- **N5 — Triggers** (`doc.triggers`, `Mc3Trigger`) — distinct from the
  `ObjectType::Area` primitive, which *is* UI-editable (see §3).
- **N6 — Scene States** (`doc.sceneStates`, `Mc3SceneState`).
- **N7 — Meta** (`doc.meta`, arbitrary key/value map).

This is the single largest coherent gap in the editor: an entire class of
format features (interactive/game-oriented metadata — scripting hooks,
audio, trigger volumes, state machines, arbitrary annotations) exists in
the format specifically to support use cases beyond static 3D scene
authoring, but the editor currently only really functions as a static-scene
authoring tool for these purposes.

## 2. Total gaps — document-level attributes

- **`rotation_units` / `euler_order`**: zero UI surface anywhere (not even
  a disabled/greyed-out field) — confirmed dead in editor rendering
  (`STAB-0701`, already tracked in plan.md) *and* dead in every settings
  panel. A user cannot set these except by hand-editing XML, and even then
  the editor's viewport ignores them.

## 3. Partial gaps — UI exists but incomplete, inconsistent, or buggy

- **IcoSphere subdivision level is not editable.** Every other
  primitive with a tessellation-density concept (segments) has a live
  slider in `PropertiesPanel.cpp`; IcoSphere instead shows a hardcoded,
  non-interactive label (`"320 triangles (2 subdivisions)"`). The
  subdivision count can only be set by hand-editing the `segments` XML
  attribute (which maps to subdivision count via a `segments/8` formula
  discovered earlier this session).
- **Primitive `axis` field has no UI** (Cylinder/Capsule/Plane's
  mesh-generation axis hint, `Mc3Primitive::axis`). XML-only.
- **`coordinate_system` UI offers an invalid value.** The Scene Properties
  panel's combo box includes `"left_handed_y_up"`, but mc3.xsd's
  `coordinateSystemType` only permits `right_handed_y_up`/`right_handed_z_up`
  — selecting it produces a document that fails XSD validation. Separately,
  and regardless of that bug: `coordinateSystem` is stored but **never
  read** anywhere in `mc3togltf/` or the rest of `src/MeshCraft/` — it's a
  UI-editable field with zero actual effect on rendering or export today.
- **Animation: `autoplay` has no UI.** `loop` has a checkbox in the
  timeline's per-action panel; the adjacent `autoplay` field
  (`Mc3Action::autoplay`) is never referenced anywhere in
  `MeshCraftApplication_Anim.cpp` — settable only via hand-edited XML/MCB.
- **Animation: new Deform/Material channels seed a hardcoded 0.0.** All 22
  `AnimatedProperty` values ARE creatable via the timeline's Add-Channel UI
  (a genuinely more complete result than the exporter-side gap might
  suggest — see the non-gaps section below), but the two functions that
  seed a new channel's initial keyframe value only handle the 10
  Position/Rotation/Scale/Visible properties; the 12
  Deform/MaterialBaseColor/MaterialRoughness/MaterialMetallic/
  MaterialEmissive properties all fall through to a hardcoded `0.0f`
  instead of reading the object's actual current value. Not a hard block —
  the user can manually correct the value after creation — but an
  inconsistency vs. how every other property type behaves.
- **Camera orthographic projection is a single-scalar data model.**
  `Mc3Camera::orthoSize` is one scalar; the editor UI fully exposes it (no
  UI gap), but the *model* can't represent separate horizontal/vertical
  half-extents. This is the root cause already flagged from the export
  side as `STAB-0695` (glTF orthographic cameras need `xmag`/`ymag`
  separately) — same underlying limitation, visible from both ends.
- **No native texture file-browse dialog.** Textures are importable via OS
  drag-and-drop (works, and hooks into the material texture-slot UI
  directly) or by manually typing a URI/path — there's no traditional
  "Browse..." file-picker button as an alternative.
- **No whole-scene Import menu.** OBJ files are only reachable indirectly,
  as a per-object `<mesh src="...">` path typed/browsed into an *existing*
  Mesh object inside an already-open mc3 document. There is no "File →
  Import" feature to bring in a whole OBJ (or any other format) as a new
  scene or a new top-level object.
- **No non-glTF export from the GUI.** File menu offers "Export GLB",
  "Export Selection...", and "Export Subtree as Template..." — all via the
  same glTF/GLB exporter. No OBJ/FBX/other export path exists in the
  editor UI (the mc3togltf *CLI tool* is glTF/GLB-only by design, but
  that's a separate, already-understood scope).
- **Undo/redo coverage is a manual discipline, not a structural
  guarantee.** The undo system is whole-document-snapshot based (deep-copy
  the entire document on `pushUndo()`), not a per-operation Command
  pattern despite the file naming. Coverage is broad (100+ call sites
  across the codebase, including CSG/material/animation edits, all spot-
  checked as present), but because it depends on every mutation call site
  remembering to call `pushUndo()` first rather than a structural
  guarantee, isolated gaps can't be ruled out without an exhaustive
  per-mutation-site audit — not attempted here, flagged as a risk area
  rather than a confirmed gap.
- **Group creation is missing from the main Add menu** (only reachable via
  a left-panel quick-add button and a "Group Selection" command) — a minor
  discoverability issue, not a capability gap.
- **Area objects have no dedicated properties UI** — they fall through to
  the generic (unlabeled) Box size editor, since `ObjectType::Area` still
  carries a `size`-only primitive under the hood. Functional, but
  cosmetically unclear to a user (looks like a plain box editor, not
  something area-specific).

## 4. Non-gaps worth noting (turned out more complete than expected)

- **Materials**: essentially complete. All scalar fields (base_color,
  roughness, metallic, alpha_mode/cutoff, double_sided, emissive_color)
  *and* all 5 texture slots (base_color/normal/emissive/
  metallic_roughness/occlusion, including `normal_scale`/
  `occlusion_strength`) are editable. Earlier session memory describing
  this as "scalars-only" (`AUDIT-0055`) is now stale/inaccurate for the
  current code.
- **Lights**: complete for all 4 types (Ambient/Directional/Spot/Point),
  matching mc3.xsd's per-type field applicability exactly.
- **Cameras**: complete, including a working Perspective/Orthographic
  toggle (contrary to what the export-side finding alone might suggest —
  the UI is fine, the underlying data model is the actual limitation, see
  §3).
- **Extrude**: fully supported — all 5 cross-section types × all 5 path
  types (25 combinations) are reachable and editable through the UI.
- **CSG**: fully supported — Union/Difference/Intersection creation, live
  type switching, cutter-role toggling (with correct undo integration),
  and nesting (via the standard hierarchy drag-drop) all work.
- **Groups/Definitions/Instances**: full CRUD for all three via UI.
- **Animation interpolation**: all 3 modes (Step/Linear/CubicBezier) are
  selectable per-keyframe, including both cubic-bezier tangent handles.
- **`model`/`unit`/`default_camera` document attributes**: all editable via
  a Scene Properties panel.

---

## Summary table

| Area | Status |
|------|--------|
| N1 SVG textures | ❌ no UI |
| N2 Embedded glTF | ❌ no UI |
| N3 Scripts | ❌ no UI |
| N4 Sounds/Music | ❌ no UI |
| N5 Triggers | ❌ no UI |
| N6 Scene States | ❌ no UI |
| N7 Meta | ❌ no UI |
| `rotation_units`/`euler_order` | ❌ no UI anywhere (also dead in rendering) |
| IcoSphere subdivision level | ❌ no UI (hardcoded label) |
| Primitive `axis` (Cylinder/Capsule/Plane) | ❌ no UI |
| `coordinate_system` | 🟡 UI exists but offers an XSD-invalid option, and the value is never read |
| Animation `autoplay` | ❌ no UI |
| Animation Deform/Material channel initial value | 🟡 hardcoded 0.0 seed, not read from object |
| Camera orthographic `xmag`/`ymag` | 🟡 UI complete, data model is single-scalar (STAB-0695) |
| Texture import | 🟡 drag-drop/manual path only, no file-browse dialog |
| Whole-scene Import (OBJ etc.) | ❌ no menu entry |
| Non-glTF Export | ❌ GLB/glTF only |
| Undo/redo | 🟡 broad but not structurally guaranteed |
| Group in Add menu | 🟡 minor discoverability gap |
| Area properties panel | 🟡 cosmetic (generic Box UI) |
| Materials, Lights, Cameras (core), Extrude, CSG, Groups/Definitions/Instances, Interpolation, model/unit/default_camera | ✅ complete |
