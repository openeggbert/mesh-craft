# MeshCraft Renderer — Analysis and Improvement Proposals

This document describes the state of the MeshCraft renderer, the root causes
of known visual problems (flat shading, poor edge overlay appearance), and
concrete proposals for improvement.

> **Status update (2026-07-19):** P1 and P2 are **implemented** — see each
> proposal's own note below. Both landed via the sibling `mesh-world` repo's
> own R-series work (commits `84b8c1a`/`3c33ba6`, outside this repo's own
> `plan.md` tracking — see `NEXT.md`'s "MeshWorld R-series cross-repo" note),
> not as a `plan.md`-tracked task in this repo. "Problem 1"/"Problem 2" below
> describe the PRE-fix state and are kept for historical context; the actual
> current behavior is described in each proposal's status note. P3-P6 remain
> open, unimplemented proposals.

---

## Current renderer architecture

The renderer is built entirely on `BasicEffect` (CNA / XNA 4.0 abstraction) and
two vertex formats:

| Format | Contains | Used for |
|--------|----------|----------|
| `VertexPositionColor` | position + per-vertex color | all solid primitives (flat path) |
| `VertexPositionNormalTexture` | position + normal + UV | textured objects only |

Every unit shape (box, sphere, cylinder, …) is built in
`SceneRenderer_Builders.cpp`. Each shape has both a `VPC` buffer (flat-color
draw) and a `VPNT` buffer (texture-capable draw with proper face normals).
`drawAuto()` picks `drawMeshTextured` when a material has a texture, otherwise
`drawMesh`.

---

## Problem 1 — Lighting is never enabled (root cause of flat appearance)

### What the code does

`BasicEffect` is initialised once at startup:

```cpp
// SceneRenderer.cpp:288
effect_ = std::make_unique<BasicEffect>(device_);
effect_->VertexColorEnabled = true;   // flat per-vertex color, no lighting
```

`LightingEnabled` is **never set to `true`** in the solid draw path. The only
place `setLightingEnabledProperty` is called is an emissive-glow helper that
explicitly turns it **off** before drawing and restores the previous value
(which is always `false`).

Result: every face of every primitive is painted with one flat color — the
material's `baseColor` — regardless of face orientation, camera angle, or any
scene lights.

### Why VPNT normals are wasted today

All unit shapes already have `VPNT` buffers with correct per-face normals
(24 verts per box face, outward normals per sphere ring, etc.).  These normals
are used **only** when a texture is present — and even then, `LightingEnabled`
is still `false`, so the normals have no effect on shading.

### Visible symptoms

- All faces of the same material are identically coloured, regardless of
  whether they face the sun, are in shadow, or face the viewer.
- No ambient-to-diffuse gradient, no highlights, no specular.
- The scene looks like a flat-colour paper model.

---

## Problem 2 — Edge overlay on flat-shaded geometry looks broken

### What happens

The edge overlay (`drawEdgeOverlay` → `drawObjectEdges`) draws a black wire
cage slightly larger than each object (`kPush = 1.003f` local-space scale) on
top of the solid mesh:

```cpp
constexpr float kPush = 1.003f;   // 0.3 % in local space
Color edgeColor(0, 0, 0, 220);    // solid black, high alpha
```

On a properly lit scene, cage edges help the eye perceive shape because each
polygon face underneath already has a shading gradient. Here, each face is a
solid block of colour. Black lines drawn over a flat-coloured quad produce a
result that looks like a painted wireframe, not a 3D surface.

### Secondary issue — `kPush` is insufficient for large objects

`kPush = 1.003f` means a 0.3 % scale push in the object's **local** space.
For a castle wall with a world scale of 30 × 10 × 2 units, the effective
world-space push on the 30-unit axis is only 0.045 units. At medium camera
distances the depth buffer cannot distinguish the cage from the face behind it,
causing z-fighting: black fragments flicker randomly across large surfaces.

### Visible symptoms

- Dense black grids covering every object simultaneously.
- Cage lines of one object overlap the solid fill of adjacent objects.
- Large flat surfaces (walls, floors) show random black flickering patches.
- The lack of lighting becomes immediately obvious because you can see every
  polygon boundary without any shading variation between them.

---

## Problem 3 — Wire cage geometry is not topology-aware

`drawObjectEdges` maps each primitive type to a pre-built `WireShape` (e.g.
`wireShapeBox_` = 12 edges of a unit cube, `wireShapeSphere_` = full
lat-lon grid of 32 × 16 rings).

This means:
- A **box** always shows all 12 outer edges, even if some are interior edges of
  a larger CSG result.
- A **sphere** shows the full latitude / longitude grid (512+ line segments per
  sphere at LOD 0), which is very noisy at distance.
- **Mesh objects** (GLB) fall through to `default:` and get a bounding-box cage
  instead of their actual topology.

---

## Proposals

The proposals are ordered from lowest to highest implementation effort.

### P1 — Enable a single ambient + directional light (low effort) — ✅ DONE

**Status (2026-07-19):** implemented (commit `84b8c1a`, "R1: Enable 3-point
directional lighting for solid primitive draw pass"). `SceneRenderer.cpp`
calls `effect_->EnableDefaultLighting()` once at setup and toggles
`setLightingEnabledProperty(true)`/`(false)` around the solid draw pass vs.
the edge-overlay/gizmo passes, matching this proposal's own step 3.

Enable `LightingEnabled` in the solid pass and add one key light (sun) plus
ambient. This requires no vertex format change — VPNT buffers already exist
for all unit shapes, and `drawMeshTextured` already selects the VPNT path.

The change is:
1. In `draw()`, before the object loop, call `EnableDefaultLighting()` (or
   manually set DirectionalLight0 direction, diffuse, specular) and set
   `LightingEnabled = true`.
2. In `drawAuto`, always call `drawMeshTextured` (not only when a texture is
   present), passing `tex = nullptr` when there is no texture. The VPNT path
   with `VertexColorEnabled = false` and a `DiffuseColor` uniform already
   handles the no-texture case.
3. After the draw loop, restore `LightingEnabled = false` so the edge overlay
   and gizmo passes are unaffected.

Expected result: each face's shading varies with its angle to the light. Boxes
show distinct top/side/bottom tones. Spheres show a smooth highlight. Edge
overlay then looks natural.

**Note:** `BasicEffect` lighting uses Phong per-vertex in XNA 4.0. This gives
faceted shading (each face has a single normal value, which is correct for
low-poly art style). If CNA exposes per-pixel lighting controls (`PreferPerPixelLighting`),
that flag can also be set for smoother gradients on curved surfaces.

### P2 — Dynamic world-space push for edge overlay (low effort) — ✅ DONE

**Status (2026-07-19):** implemented (commit `3c33ba6`, "R1/R2 lighting +
edge fix..."). `SceneRenderer_Extrude.cpp`'s `kPush` is now
`std::clamp(1.0f + 0.25f / std::max(maxScale, 0.001f), 1.003f, 1.02f)` — a
world-scale-derived push clamped to a sane range, not the fixed `1.003f`
this proposal originally flagged. (Applied to the Extrude draw path; not
independently re-verified against every other primitive's own edge-overlay
call site.)

Replace the fixed `kPush = 1.003f` with a value derived from the actual world
scale of the object. The goal is a constant-size push in world space rather
than a percentage of local size:

```cpp
// Target: ~0.02 world units push on each axis
float worldScaleApprox = /* max(world.M11, world.M22, world.M33) absolute */ ;
float kPushWorld = 1.0f + (0.02f / std::max(worldScaleApprox, 0.001f));
```

Alternatively, add a fixed depth bias in clip space (a clip-Z offset) which
avoids the scale dependency entirely. If CNA provides `RasterizerState` with
`DepthBias` / `SlopeScaleDepthBias`, that is the cleanest solution.

### P3 — Normal-shaded edge colour (medium effort, current API)

Instead of black for all edges, tint the edge colour based on the average
normal of the face it lies on, mapped to a grey scale:

```
grey = dot(faceNormal, lightDir) * 0.5 + 0.5
edgeColor = lerp(darkEdge, lightEdge, grey)
```

This makes edges on sun-facing faces lighter and edges on shadowed faces
darker, giving the cage a depth-aware appearance even without per-face solid
shading.

### P4 — Silhouette-only edge overlay (medium effort, current API)

Instead of drawing all 12 edges of every box, compute and draw only
**silhouette edges** — edges shared between a front-facing and a back-facing
triangle relative to the camera. This dramatically reduces line density (516
objects × 12 edges → only a handful per visible face) and produces the clean
"outline" look used in stylised 3D editors.

The algorithm requires iterating face adjacency; this can be precomputed per
unit shape and cached. At 516 objects, even a per-frame CPU computation is
fast enough.

### P5 — Custom per-face flat shading shader (medium effort, future CNA API)

Once CNA exposes a shader upload API (beyond XNA 4.0 BasicEffect), a simple
GLSL/HLSL fragment shader can compute per-face shading with:

```glsl
vec3 faceNormal = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
float ndl       = max(dot(faceNormal, uLightDir), 0.0);
fragColor       = uMaterialColor * (uAmbient + ndl * uLightColor);
```

This gives true flat-shaded facets without needing to store normals in the
vertex buffer at all (normals are reconstructed from screen-space derivatives).
It is the correct long-term solution for a low-poly art style.

### P6 — Screen-space edge detection post-pass (higher effort, future CNA API)

Once CNA provides render-target (FBO) support and custom shaders, a screen-
space edge detection pass (Sobel on the depth buffer and/or normal buffer)
produces edges that are:
- camera-distance-independent (no z-fighting, no kPush)
- topology-agnostic (works on mesh, CSG result, extrude, GLB)
- controllable via threshold parameter
- compatible with any lighting model

This is the standard technique used in Blender's viewport overlay and most
modern 3D editors. It is the target architecture for MeshCraft's edge overlay
once the CNA API provides the necessary primitives (FBO read/write, multi-pass,
fragment shader).

---

## Recommended action order

| Priority | Proposal | Effort | Prerequisite | Status |
|----------|----------|--------|--------------|--------|
| 1 | P1 — enable 1 directional light + ambient | 1–2 h | none (VPNT already built) | ✅ done (`84b8c1a`) |
| 2 | P2 — world-space depth push for edges | 1 h | none | ✅ done (`3c33ba6`) |
| 3 | P3 — normal-shaded edge colour | 2–3 h | P1 (needs normals) | open |
| 4 | P4 — silhouette-only edges | half day | P1 | open |
| 5 | P5 — flat-shading fragment shader | 1 day | CNA shader upload API | open |
| 6 | P6 — screen-space edge post-pass | 2–3 days | CNA FBO + shader API | open |

P1 and P2 are done (2026-07-19 status update, above) and eliminated the
majority of the visual problems described above.
P5 and P6 are the correct long-term solutions and should be the target once
CNA extends beyond XNA 4.0.
