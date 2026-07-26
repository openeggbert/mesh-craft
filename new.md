# MeshCraft: Deep Product, Architecture, and Feature Analysis

_Prepared 2026-07-26 from the current source tree, the format specification,
the test/build configuration, and the current Release build._

## Executive summary

MeshCraft is already a substantial, safety-conscious 3D scene editor rather
than an early prototype. Its strongest property is a well-defined canonical
scene model (`Mc3Document`) that drives XML, JSON, MCB, the live editor, and a
glTF/GLB exporter. The project supports parametric primitives, hierarchy,
PBR-oriented materials, CSG, path extrusion, reusable definitions/instances,
animation, scripts, triggers, scene states, import libraries, and several
asset-metadata concepts. It also has an unusually broad regression suite for a
project of this size.

The main product opportunity is not to add isolated primitive types or more
panels. It is to make the already-authored semantics operational end-to-end:

1. turn editable library and asset metadata into a real reusable-asset
   workflow;
2. turn manually fired triggers and manually applied states into controlled
   scene events;
3. close the most important viewport/export contract gaps; and
4. broaden exchange and deployment only after the release gate is reliable.

This order gives users a stronger authoring loop without prematurely turning
the editor into a general-purpose game engine. The recommended first new
feature is **native MC3 library authoring and publishing**, followed by
**metadata-driven LOD/culling** and **event bindings for Areas/triggers**.
Those features reuse existing model fields and mostly stay within this
repository's ownership boundary.

There is one immediate quality finding that should be treated as a release
gate before feature work:

- In the current `b-release` build, `ctest -R '^mc3_ai$'` previously reached
  its 30-second CTest timeout because the mock-HTTP test helper waited forever
  for a loopback listener. The untrusted traversal-shaped `<include>` response
  is correctly ignored; its adjacent assertion was merely buffered while the
  later mock-server setup hung. This is fixed by a bounded readiness probe and
  a precise skip of only the loopback-dependent tests where such listeners are
  unavailable.

The render-test result needs a configuration caveat: a fresh CMake configure
correctly detects the unusable Xvfb display and disables the 35
render-dependent tests while leaving `render_display_preflight` as a skip.
An earlier run used stale generated CTest metadata and therefore launched the
disabled tests. This is covered by the existing completed `AUD-090` task, not
a new renderer defect.

The AI finding matters because the project rightly treats its test suite as a
major product asset. It should be resolved or explicitly quarantined before
claiming a clean release baseline.

## What MeshCraft is today

### Product shape

MeshCraft is best described as a **scene-authoring and asset-compilation tool
for the MC3 ecosystem**. MC3 stores editable constructive intent rather than
only final triangles. That lets a scene retain primitives, CSG operands,
extrude paths, definitions, material relationships, and selected gameplay
metadata after save/load.

The core pipeline is:

```text
        .mc3.xml / .mc3.json / .mcb
                    |
              Mc3Document
       _________|______________
      |            |           |
 Editor viewport   MCB      glTF / GLB
  + authoring UI  runtime      export
```

`mc3/` deliberately has no graphics dependency. `mcb/`, `mc3tomcb/`, and
`mc3togltf/` can build independently. The editor adds Dear ImGui, CNA/SDL
graphics, scene preview, commands, animation playback, auto-save, registry,
and optional AI/Lua features.

### Capabilities that are already mature

| Area | Current capability | Assessment |
|---|---|---|
| Scene model | XML and semantic JSON, MCB, schema, validation, input budgets, include/import policies, library hashes | Strong foundation for durable content |
| Geometry | Box/cube, sphere, cylinder, cone, plane, torus, capsule, disk, grid, icosphere, OBJ, embedded self-contained GLB, extrude, CSG | Rich procedural authoring set |
| Editing | Hierarchy, multi-selection, transform gizmos, snapping, property editing, grouping, arrays/scatter, rename/find-replace, macros, undo/redo, backups | Good desktop-editor baseline |
| Materials | PBR factors and texture slots, SVG rasterization, sampler metadata, normal-map tangents, texture browsing | More complete than a typical internal tool |
| Interchange | Deterministic glTF/GLB export, geometry caching, CSG evaluation, lights/cameras, transform animation, MCB conversion | Export is a first-class product surface |
| Runtime-style data | Lua sandbox, scripts, manual trigger fire, manual scene-state apply, sounds/music, sockets, reusable definitions | Useful base, but several concepts are still authoring-only |
| Quality | 185 CTest registrations in the current Release tree; fuzzing, hostile-input, round-trip, determinism, Blender import, and pixel tests | Excellent intent and broad coverage |

### Architecture strengths

- The canonical `Mc3Document` keeps the file formats and consumers aligned
  much better than a UI-owned scene model would.
- Parsers are deliberately hardened: input-size and complexity budgets,
  numerical clamps, include policy controls, and MCB corruption defenses are
  visible throughout the code and tests.
- The glTF exporter has useful professional qualities: deterministic output,
  no partial output on failure, geometry caching, texture safety checks,
  tangent generation, UV generation, and real Blender-import checks.
- The editor has already separated several narrow subsystems (selection,
  locking, bookmarks, walk controller, status, overlays, UI contexts). This
  makes focused new work safer than a monolithic application would.
- Dependency boundaries are explicit. In particular, CNA and sharp-runtime
  are sibling repositories and should not be changed here without owner
  permission.

## The important gaps

### 1. Some MC3 fields are authored but not fully consumed

This is the largest functional theme in the codebase.

| Authored capability | Current behavior | Product consequence |
|---|---|---|
| `coordinate_system=right_handed_z_up` | Stored, edited, and warned about, but not applied by the editor or exporter | A valid document can visibly mean something different from its declaration |
| Ordinary-object `uv_mapping` | Box/sphere projection and transforms are honored by `mc3togltf`; ordinary live viewport geometry uses default UVs. CSG roots have better parity | The editor preview can disagree with the exported asset |
| Point/spot lights | Exported, but the live BasicEffect preview only has directional/ambient lighting; point/spot remain largely gizmo-only | Lighting design cannot be judged faithfully in the editor |
| CSG child materials | Exporter preserves child-material primitives when the CSG root has no override; the viewport uses the root material | Preview does not show final material composition |
| Asset metadata LOD references / max visibility distance | Serialized and edited. The renderer has separate procedural tessellation LOD, but metadata definition references and visibility-distance hints are not the scene-selection contract | The format promises reusable asset tiers that authoring does not yet exploit |
| Triggers and states | Their steps/overrides work from explicit buttons; no event binding connects an Area, click, timer, or collision to a trigger/state | The data model stops short of interactive scene behavior |
| Collision labels | Walk mode supports explicit `collision="box"`; other declared proxy modes are not simulated | Walkthrough scenes cannot use their richer collision intent |

These are not all bugs. Several are documented scope choices. Together,
however, they show the correct direction for new work: complete meaningful
vertical slices from authoring through preview/export/runtime semantics.

### 2. Interchange is export-led, not import-led

The exporter is sophisticated, but incoming geometry remains intentionally
limited:

- OBJ is supported, but its per-face/material-group information is flattened
  to one MC3 material.
- `embed:` accepts bounded self-contained GLB triangle geometry with MC3
  retaining material authority; it is intentionally not a full glTF importer.
- There is no editing workflow that converts a common multi-material glTF/GLB
  asset into native MC3 objects/materials/definitions.

This is the biggest adoption barrier for artists who start in Blender or other
DCC tools. It is also an opportunity to make MeshCraft the canonical editor
for existing content rather than only new parametric scenes.

### 3. The product is desktop-Linux verified first

Linux/EASYGL is the verified editor route. Windows, Web, and Android are
partly blocked by sibling-runtime/toolchain issues. Web currently crashes on
its first resize event; Android lacks NDK/package/device validation; Windows
is blocked before the complete editor links. These are real product risks but
must be coordinated with the owners of CNA and sharp-runtime rather than
worked around by MeshCraft-only source changes.

### 4. Two independent geometry consumers increase parity cost

The live renderer and `mc3togltf` generate geometry separately except for a
small shared path for torus/capsule/icosphere. The code intentionally has
different triangle winding in preview and export. This design is workable, but
every new geometric semantic has a parity cost: transforms, UVs, CSG, LOD,
deformation, units, coordinate conventions, and material assignment must be
validated in both consumers.

The answer is not an unsafe wholesale rewrite. The answer is a small shared
semantic-evaluation layer and focused differential tests whenever a feature
needs both paths.

## Recommended roadmap

### Wave 0 — restore a trustworthy release gate

This is not new user functionality, but it is the correct first priority.

1. Diagnose and fix or isolate the `mc3_ai` timeout. Add a narrow regression
   test that identifies the actual hanging boundary and has a short,
   meaningful timeout rather than letting the entire AI test become opaque.
2. Preserve the existing render-test gating contract. A fresh configure must
   mark render-dependent tests disabled when Xvfb/display preflight fails;
   a valid display must cause the full set to run.
3. Keep the current two-part verification contract: CNA-free/non-render tests
   and an explicit Xvfb render partition. Do not treat display-less failures
   as product rendering failures.

**Acceptance criteria:** a clean Release run has deterministic pass/skip
behavior, reports why a platform-dependent partition was skipped, and does not
hide a real failure behind environmental noise.

### Wave 1 — make reusable MC3 assets a complete workflow

#### 1. Native library authoring, publishing, and import health

**Why it is the best first feature:** the model and most of the core API
already exist. Library metadata/import rows are editable, content hashes can
be computed, and imports can be resolved after load. What is missing is a
cohesive user workflow.

**Feature scope:**

- Add Open Library / Save as Library actions for `.mc3lib.xml` and
  `.mc3lib.json`, using the existing library I/O API rather than duplicating
  serializers.
- Add “Create Definition from Selection” and “Publish Definition” flows with
  validation of definition id, namespace, semantic version, and metadata.
- Provide an import browser that shows source, resolved version/hash, imported
  definition count, cycles/missing files, and collision errors.
- Add a definition picker/search panel for placing imported assets, with
  category/style/semantic-tag filtering. Start text-first; thumbnails can be a
  later enhancement.
- Warn before saving a normal scene whose imported definitions were modified
  locally in a way that will be lost or shadowed.

**Why it fits the architecture:** it primarily touches FileOps, MenuBar/UI,
`Mc3ImportResolver`, and existing library APIs. It does not need a CNA change
or a new geometry format.

**Tests:** library XML/JSON round-trip, content-hash stability, successful and
failed import resolution, duplicate/cycle diagnostics, selection-to-definition
conversion, and a headless file workflow test.

### Wave 2 — use asset metadata as executable scene intent

#### 2. Metadata-driven LOD, distance culling, and asset variants

The renderer already has a useful procedural three-tier tessellation LOD for
some primitives. Separately, `assetMetadata.lods` maps named tiers to
definition ids and `max_visibility_distance` exists in the format. These
should become one explicit asset-level system.

**Feature scope:**

- Resolve an instance’s definition metadata into near/mid/far definitions.
- Choose a tier from camera distance with configurable thresholds and
  hysteresis, so objects do not flicker at boundaries.
- Respect `max_visibility_distance` as a distance-culling hint, with an editor
  debug overlay that shows selected tier, distance, and cull reason.
- Preserve deterministic instance variants: use a stable seed derived from
  document/object identity rather than per-frame random selection.
- Add an optional glTF export policy: export the selected/default tier for
  maximum compatibility first; add a glTF LOD extension only as an explicit
  advanced option after target viewers are tested.

**Value:** large towns, forests, interiors, and imported libraries can become
fast enough to edit and preview without destroying authoring detail.

**Important constraint:** do not conflate this with the existing primitive
tessellation LOD. Asset LOD changes the selected definition; primitive LOD
changes triangle density. Both may coexist and need clear UI labels.

#### 3. Collision proxy expansion

Extend walk-mode collision from box-only to selected, explicit proxy modes:

- sphere/capsule for characters and rounded props;
- simple triangle-mesh or convex proxy for static floors/walls, with a
  strict triangle budget and explicit opt-in;
- visual debug rendering of active collision shapes;
- a “Generate simple proxy” command for supported primitives.

This makes the existing walkthrough mode useful for level validation while
maintaining the project’s principle of not silently approximating authored
collision semantics.

### Wave 3 — make scenes react to user/game events

#### 4. Event bindings for Areas, objects, triggers, and states

Today an Area is visually a box-shaped marker and triggers can be fired only
manually. This is enough for testing scripts/actions but not enough to author
an actual interactive scene.

**Recommended design:** introduce a small explicit event-binding model rather
than relying on name matching. A binding should identify a source object or
Area, an event (`enter`, `exit`, `click`, `timer`, perhaps `action-finished`),
and a target trigger or scene state.

**First slice:**

- `on_enter` / `on_exit` for Area volumes and a timer binding;
- per-binding enabled flag, cooldown/debounce, and a deterministic one-shot
  option;
- visual overlay for activated volumes and a test/simulate button;
- trigger execution remains ordered and uses the existing Lua sandbox;
- applying a state should be a named action in the same binding system.

**Safety and semantics:** event dispatch must have a frame-local recursion
guard, an execution budget, clear errors for dangling target ids, and an
undo-free play/simulate mode so testing a scene does not accidentally rewrite
the authored document. Persisting mutable gameplay state should be a separate
decision.

**Export note:** this is MC3/MCB runtime semantics; glTF has no equivalent.
The exporter should preserve its current explicit warning/omission behavior.

### Wave 4 — reduce “preview looks different from export” surprises

#### 5. Coordinate-system support as a scene convention feature

`right_handed_z_up` should either become operational or be removed from the
authoring UI; the better product choice is to support it.

Implement one well-tested scene-convention transform at the semantic boundary,
then apply it consistently to rendering, picking, gizmos, camera views,
world-position UI, CSG, walk collision, and glTF node transforms. Include a
clear import/open notice and an optional one-way “normalize to Y-up” command
for teams that want a fixed house convention.

The risk is high because transform paths currently live in more than one
consumer. Mitigate it with a small shared convention helper and fixtures whose
world positions, screenshot orientation, picking, and glTF transforms are all
checked together.

#### 6. Preview/export visual-fidelity program

Deliver this as small vertical slices, not as a renderer rewrite:

1. ordinary primitive UV mapping in the viewport, including box/sphere
   projection, scale, offset, and rotation;
2. point and spot light preview with attenuation/cones, using the authored
   light values rather than only gizmos;
3. CSG child-material display parity when no root override is authored;
4. a material-preview/viewport mode that makes normal, occlusion, emissive,
   alpha, and sampler choices visible enough for authors to trust the result.

The preferred implementation is a capability-gated CNA `ShaderEffect` path
with a clearly documented fallback on unqualified backends. It should not
reintroduce raw OpenGL calls or overpromise Vulkan/WebGPU parity before those
backends are pixel-qualified.

### Wave 5 — broaden exchange without losing MC3 intent

#### 7. Incremental import pipeline: multi-material OBJ first, glTF next

Start with a narrow problem that users hit immediately: preserve OBJ groups
and material assignments as multiple MC3 mesh objects/materials instead of
flattening the asset to one material. Then add a deliberate glTF/GLB import
workflow:

- import self-contained GLB first; make external-resource glTF an explicit
  trusted-mode capability;
- map nodes, meshes, cameras, punctual lights, material factors, images, and
  transforms to native MC3; report unsupported skins/morphs/animations rather
  than silently deleting them;
- offer two modes: editable imported objects, or a bounded embedded mesh for
  fast/reference use;
- use the current safe embedded-GLB loader as a starting point, but do not
  mistake it for a full importer.

This feature has high adoption value and should arrive after the library
workflow, because an imported object is much more useful when it can be
published as a reusable MC3 definition.

#### 8. Export optimization controls

The Export dialog visibly exposes disabled “Quantize meshes” and texture
embedding options. Make advanced controls truthful and useful:

- add opt-in `KHR_mesh_quantization` with documented precision/error bounds
  and a structural GLB test;
- clearly state the texture behavior for `.gltf` versus `.glb`, then expose
  only controls that the exporter actually honors;
- add mesh/texture/output-size estimates before export;
- provide an export report with unsupported/downgraded feature counts and
  direct object ids.

Compression must be opt-in and deterministic. Avoid adding Draco or other
heavy codecs until there is a distribution and compatibility decision.

### Wave 6 — authoring productivity and ecosystem features

#### 9. Animation clips, playback control, and export policy

The timeline supports transform, visibility, material, and deform channels,
but glTF core only receives transform animation. A pragmatic expansion is:

- named clip ranges, playback speed, reverse/loop and transition previews;
- a non-destructive “bake for export” command that samples unsupported
  animation into an explicitly chosen compatible representation where viable;
- optional `KHR_animation_pointer` export for material/visibility-like
  channels only when the user accepts limited viewer support;
- clear per-channel export diagnostics rather than silently skipping data.

Do not claim that every MC3 animation has a universal glTF equivalent. The
editor can remain the full-fidelity authoring surface while exporting a
well-documented subset.

#### 10. Model Registry v2 and asset packs

The local SQLite registry is useful but intentionally lacks thumbnails and
sync. A practical next version would add generated thumbnail caching,
metadata/tag search, dependency-aware pack export, license/provenance views,
and duplicate/unused-material reports. Cloud synchronization should be a
separate product and security decision, not an incidental SQLite change.

#### 11. History and review tooling

The current 20-entry whole-document deep-copy undo stack is safe and simple,
but it limits long authoring sessions. A future user-facing productivity
feature could add memory-budgeted history, named checkpoints, scene diff, and
restore points. Implement it as a dedicated subsystem; it should not weaken
the existing reliable undo semantics merely to store more entries.

## Cross-cutting implementation guidance

### Keep one semantic traversal contract

Do not immediately merge the renderer and exporter into a giant shared mesh
library. Their output requirements differ, including intentional winding.
Instead, introduce small CNA-free helpers for concepts that must agree:

- document coordinate convention;
- object/instance transform accumulation;
- definition/variant/LOD resolution;
- effective visibility and material resolution;
- stable object identity for diagnostics and deterministic random choices.

Each helper should have direct tests and at least one differential editor versus
export fixture when its result affects geometry.

### Treat format evolution as a product contract

Unknown XML attributes/elements are currently silently dropped by a load/save
cycle. New event bindings, LOD policy, importer provenance, and advanced
export metadata should therefore be versioned deliberately. Before adding any
new MC3 field:

1. update the AST, XML/XSD, JSON, and MCB surfaces together;
2. define old-reader behavior and a user-visible warning where data could be
   lost;
3. add XML/JSON/MCB round-trip and malformed-input tests; and
4. update `mc3togltf`/editor support matrices honestly.

### Preserve safety defaults

The project already makes good security choices for untrusted paths, embeds,
SVGs, and scripts. New import, event, and library features should retain this
posture: trusted-mode toggles must be explicit, resource budgets must be
enforced before allocation, and scripts/events need bounded execution.

### Keep platform claims evidence-based

Feature work that needs new CNA behavior should be proposed to the CNA owner
with a minimal required API and a testable acceptance case. MeshCraft should
not duplicate raw graphics code to bypass an unqualified backend. Web and
Android packaging are valuable delivery milestones, but are separate from a
MeshCraft-only feature implementation until their sibling blockers are fixed.

## Suggested delivery sequence

| Milestone | Deliverable | Why this order |
|---|---|---|
| 0 | Fix AI-test timeout and robust render-test gating | Restores trustworthy evidence for later work |
| 1 | Native library publish/open/import-health workflow | High user value, leverages stable existing APIs, low graphics risk |
| 2 | Metadata-driven definition LOD and distance culling | Converts existing asset metadata into visible performance value |
| 3 | Event bindings for Areas/triggers/states | Converts authored scene data into controlled interactivity |
| 4 | Coordinate convention plus first preview/export parity slices | Removes the most surprising correctness gaps before visual expansion |
| 5 | Multi-material OBJ and self-contained GLB import | Expands adoption once imported content can be published as libraries |
| 6 | Collision proxies, export optimization, richer animation/registry work | Valuable, but larger design/compatibility surface |

For the next implementation decision, choose **Milestone 1**. It has clear
user-facing results, uses mature components already present in the repository,
requires no CNA change, and creates a strong foundation for LOD, imported
assets, and a better registry.

## Work deliberately not recommended yet

- A wholesale `MeshCraftApplication` refactor. The current phased extraction
  strategy is safer and already has explicit boundaries.
- CNA or sharp-runtime source changes without owner approval.
- A full game runtime/networking system. Event bindings should first serve
  authoring, preview, and MC3/MCB semantics.
- Generic modifier stacks, arbitrary mesh editing, skeletal animation, or
  collaboration sync in one release. Each would add a much larger data-model
  and compatibility commitment than the recommended milestones.
- Claiming Windows, Web, Android, Vulkan, or WebGPU feature parity without a
  platform build and pixel-qualified run.

## Conclusion

MeshCraft has the ingredients of a focused, high-quality scene editor for an
asset-driven game ecosystem: an expressive editable format, hardened parsers,
strong export machinery, and a serious test culture. Its next stage should be
defined by **semantic completion**, not feature count. Make reusable assets
easy to publish and consume, make authored metadata affect preview/runtime,
and make the preview reliably represent exported results. That sequence will
make every later feature—imports, LOD-heavy worlds, interactive scenes, and
platform delivery—more coherent and less expensive to maintain.
