# MeshCraft capability matrix

_Last verified against source: 2026-07-26._ This is the authoritative concise
capability matrix. `MC3_FORMAT.md` is the normative format reference,
`README.md` is the product overview, and `TESTING.md` explains verification;
none should duplicate a competing platform or feature-status matrix.

| Capability | MC3 format / tooling | Live editor | Platform / scope |
| --- | --- | --- | --- |
| MC3 XML and semantic JSON | Parse, validate, and write | Open and save both forms | CNA-independent core |
| MCB | Read and write, with optional compression when built with zlib | Open/save through the shared document model | CNA-independent core |
| glTF/GLB | Export supported geometry, materials, cameras, lights, and supported animation channels | GLB/glTF can be imported as bounded editable MC3 content | Event bindings and other MC3 runtime data are omitted with a warning |
| Coordinate system | Y-up and Z-up are represented and preserved | Y-up/Z-up conversion is applied to rendering, picking, gizmos, collision, cameras, and lights | Normalization to Y-up is explicit |
| Rotation units and Euler order | XML/JSON/MCB and glTF honor `rotation_units` and `euler_order` | Rendering, CSG, picking, gizmos, cameras, Walk Mode and animated transforms honor the authored convention | Static scenes can explicitly normalize to degrees/XYZ; animated Euler channels are retained to avoid lossy conversion |
| Ordinary-object UV mapping | Default, box, and sphere projection export | Default, box, and sphere projection are previewed for normal/UV geometry | CSG mapping has a separate cache/path |
| Walk collision proxy | Object `collision` labels are round-tripped | `box`, uniform `sphere`/IcoSphere, and compatible upright `capsule` are exact supported proxies | `mesh`, `convex`, incompatible shapes, and excess proxies are reported/ignored, never silently boxed |
| Autosave recovery | Autosave is a sibling of the saved MC3 file | A newer autosave opens **Recover Unsaved Changes** with Recover and Discard actions | Recovery keeps the document dirty until an explicit save |
| Web persistent filesystem | Emscripten pre-JS mounts IDBFS at its web home directory and syncs it on startup/unload | Not independently end-to-end qualified in this checkout | Web rendering/runtime remains blocked by the CNA resize failure |
| Alternate backends and Android | Source configuration selects the intended EASYGL route | No release qualification claim | `SYS-W8-05` and `AUD-042` require external tooling/owner evidence |
| Lua scripts and event bindings | Scripts, triggers, states, and bindings round-trip | Explicit Preview/Play executes timer, Walk Mode Area enter/exit, and picked-object click bindings. Each dispatch batch uses one isolated, validated document transaction with the 16 MiB VM and 50-million-instruction script limits. | Normal editing does not dispatch bindings; this is bounded preview, not a general game runtime. |

## Evidence anchors

- Walk proxy dispatch is in `src/MeshCraft/Application/WalkMode.cpp`.
- Autosave detection/recovery/discard are in
  `src/MeshCraft/Application/FileOps.cpp`; the modal is in
  `src/MeshCraft/Application/UI/Overlays.cpp`.
- The Emscripten persistence bootstrap is `cmake/web/pre.js` and is linked
  from the root `CMakeLists.txt` with `-lidbfs.js`.
- Ordinary-object viewport UV mapping is covered by
  `uv_mapping_viewport_test`; exporter behavior is documented in
  `MC3_FORMAT.md`.

`test/validate_capability_documentation.py` checks the bounded claims above
against these source anchors. It intentionally does not encode CTest totals,
benchmark times, or unverified platform-runtime claims.
