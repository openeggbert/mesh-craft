#!/usr/bin/env python3
"""Verify mc3togltf output for all_primitives.mc3.xml and all_objects.mc3.xml."""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


EXPECTED_PRIMITIVE_NODES = [
    "Box", "Cube", "Sphere", "Cylinder", "Cone", "Plane",
    "Torus", "TorusThin", "Capsule", "Disk", "RingDisk", "Grid", "IcoSphere",
]

EXPECTED_OBJECTS_NODES = [
    "Tetra", "Pipe", "MyGroup", "GroupSphere", "GroupBox",
    "CubeA", "CubeB", "TriggerZone",
]


def node_map(gltf):
    return {n.get("name", ""): n for n in gltf.get("nodes", [])}


def check_node_has_geometry(gltf, name):
    """Verify a named node has a mesh with POSITION accessor; return (mesh, prim)."""
    nmap = node_map(gltf)
    assert name in nmap, f"Node '{name}' missing from GLTF output"
    node = nmap[name]
    mesh_idx = node.get("mesh")
    assert mesh_idx is not None, f"Node '{name}' has no mesh field"
    meshes = gltf.get("meshes", [])
    assert 0 <= mesh_idx < len(meshes), \
        f"Node '{name}' mesh idx {mesh_idx} out of range (have {len(meshes)})"
    prims = meshes[mesh_idx].get("primitives", [])
    assert len(prims) > 0, f"Node '{name}' mesh has no primitives"
    attrs = prims[0].get("attributes", {})
    assert "POSITION" in attrs, f"Node '{name}' primitive[0] missing POSITION attribute"
    return meshes[mesh_idx], prims[0]


def accessor_count(gltf, acc_idx):
    accs = gltf.get("accessors", [])
    if 0 <= acc_idx < len(accs):
        return accs[acc_idx].get("count", 0)
    return 0


def test_all_primitives(mc3togltf, xml_path, tmpdir):
    out_gltf = os.path.join(tmpdir, "out_primitives.gltf")
    r = run([mc3togltf, xml_path, out_gltf])
    assert r.returncode == 0, f"mc3togltf failed:\n{r.stderr}"
    assert os.path.exists(out_gltf) and os.path.getsize(out_gltf) > 0, \
        "Output GLTF is empty"

    with open(out_gltf) as f:
        gltf = json.load(f)

    assert gltf.get("asset", {}).get("version") == "2.0", \
        f"asset.version != '2.0', got: {gltf.get('asset', {}).get('version')}"

    # Every expected primitive must have a node with a POSITION accessor
    # and a non-zero index count (no zero-triangle meshes).
    for name in EXPECTED_PRIMITIVE_NODES:
        mesh, prim = check_node_has_geometry(gltf, name)
        pos_idx = prim["attributes"]["POSITION"]
        vc = accessor_count(gltf, pos_idx)
        assert vc > 0, f"Node '{name}' POSITION accessor has 0 vertices"
        # STAB-0163: verify triangle count > 0
        idx_acc = prim.get("indices")
        assert idx_acc is not None, f"Node '{name}' primitive has no indices accessor"
        ic = accessor_count(gltf, idx_acc)
        assert ic > 0, f"Node '{name}' indices accessor has 0 entries (zero triangles)"
        assert ic % 3 == 0, f"Node '{name}' index count {ic} is not a multiple of 3"

    # STAB-0199: node count must equal object count in the input XML (no silent drops)
    EXPECTED_OBJECT_COUNT = len(EXPECTED_PRIMITIVE_NODES)  # 12 objects in all_primitives.mc3.xml
    actual_nodes = len(gltf.get("nodes", []))
    assert actual_nodes >= EXPECTED_OBJECT_COUNT, (
        f"STAB-0199: expected at least {EXPECTED_OBJECT_COUNT} nodes (one per object), "
        f"got {actual_nodes} — some objects may have been silently dropped"
    )

    # Total mesh count must cover all primitives
    assert len(gltf.get("meshes", [])) >= len(EXPECTED_PRIMITIVE_NODES), (
        f"Expected at least {len(EXPECTED_PRIMITIVE_NODES)} meshes, "
        f"got {len(gltf.get('meshes', []))}"
    )

    # RingDisk must have more vertices than solid Disk (different topology)
    # all_primitives.mc3.xml: both use segments=32
    # solid Disk: 1 center + 33 rim = 34 verts
    # ring RingDisk: (32+1) outer + (32+1) inner = 66 verts
    _, disk_prim   = check_node_has_geometry(gltf, "Disk")
    _, ring_prim   = check_node_has_geometry(gltf, "RingDisk")
    disk_vc = accessor_count(gltf, disk_prim["attributes"]["POSITION"])
    ring_vc = accessor_count(gltf, ring_prim["attributes"]["POSITION"])
    assert ring_vc > disk_vc, (
        f"RingDisk should have more vertices than solid Disk "
        f"(ring={ring_vc}, disk={disk_vc})"
    )

    # STAB-0162: Torus must actually use Mc3Primitive::minorRadius (tube
    # radius) from the XML, not ignore it. Torus (minor_radius=0.15) and
    # TorusThin (minor_radius=0.05) share the same major_radius/segments, so
    # if minorRadius were ignored their meshes would be identical. Compare
    # each accessor's own min/max bounds (mesh-local space, unaffected by the
    # node's position translation) rather than vertex *count*, since a
    # different minorRadius changes the ring's radial extent, not its
    # topology/vertex count.
    _, torus_prim      = check_node_has_geometry(gltf, "Torus")
    _, torus_thin_prim = check_node_has_geometry(gltf, "TorusThin")
    accs = gltf.get("accessors", [])
    torus_acc      = accs[torus_prim["attributes"]["POSITION"]]
    torus_thin_acc = accs[torus_thin_prim["attributes"]["POSITION"]]
    torus_extent      = max(abs(v) for v in torus_acc["max"] + torus_acc["min"])
    torus_thin_extent = max(abs(v) for v in torus_thin_acc["max"] + torus_thin_acc["min"])
    assert torus_extent > torus_thin_extent, (
        f"Torus (minor_radius=0.15) should have a larger radial extent than "
        f"TorusThin (minor_radius=0.05): got {torus_extent} vs {torus_thin_extent} "
        f"— minorRadius may not be applied"
    )

    # GLB magic-byte check
    out_glb = os.path.join(tmpdir, "out_primitives.glb")
    r = run([mc3togltf, xml_path, out_glb])
    assert r.returncode == 0, f"mc3togltf GLB failed:\n{r.stderr}"
    with open(out_glb, "rb") as f:
        magic = f.read(4)
    assert magic == b"glTF", f"GLB magic mismatch: {magic!r}"

    print("all_primitives_export_test: PASS")


# AUDIT-0012: cross-check mc3togltf's buildPrimitive() output (exercised here
# via the real mc3togltf CLI, reading the exported glTF accessor min/max in
# mesh-local space) against SceneRenderer's independent "unit mesh + scale
# matrix" implementation of the same primitive types (SceneRenderer.cpp's
# drawObject(), lines ~637-713) -- WITHOUT needing a live GraphicsDevice/CNA
# context, which SceneRenderer requires and this headless Python test cannot
# provide. Instead, this encodes the analytically-derived expected bbox for
# each primitive type from SceneRenderer's own scale-formula + unit-mesh
# baseline dimensions (every unit shape in SceneRenderer_Builders.cpp has
# radius/half-extent exactly 0.5, confirmed by reading each buildUnitX()),
# and asserts mc3togltf's real output matches. If either implementation's
# formula silently diverges (e.g. a future change to one side's scale math),
# this test will catch the resulting bbox mismatch -- the same silent-
# divergence risk class already documented for the CSG dual-path invariant.
#
# Expected values below are derived from all_primitives.mc3.xml's own
# parameters combined with SceneRenderer's documented scale formulas:
#   Box/Cube:  half-extent = size/2                         (SceneRenderer.cpp:643)
#   Sphere:    half-extent = radius (all axes)               (SceneRenderer.cpp:647-649)
#   Cylinder:  half-extent = (radius, height/2, radius)      (SceneRenderer.cpp:653-660)
#   Cone:      half-extent = (radius, height/2, radius)      (SceneRenderer.cpp:664-667)
#   Plane:     half-extent = (size.x/2, 0, size.z/2)         (SceneRenderer.cpp:671-673)
#   Torus:     half-extent = (major+minor, minor, major+minor) (SceneRenderer.cpp:677-683)
#   Capsule:   half-extent = (radius, height/2 + radius, radius) (SceneRenderer.cpp:686-691)
#   Disk:      half-extent = (radius, 0, radius) -- outer radius only
#   Grid:      half-extent = (size.x/2, 0, size.z/2)
#   IcoSphere: half-extent = radius (all axes)                (SceneRenderer.cpp:710-711)
EXPECTED_BBOX_HALF_EXTENT = {
    "Box":        (0.5,  0.5,  0.5),
    "Cube":       (0.5,  0.5,  0.5),   # default cube side=1.0 (Mc3Primitive::cube())
    "Sphere":     (0.5,  0.5,  0.5),   # radius=0.5
    "Cylinder":   (0.4,  0.5,  0.4),   # radius=0.4, height=1.0
    "Cone":       (0.5,  0.5,  0.5),   # radius=0.5, height=1.0
    "Plane":      (1.0,  0.0,  1.0),   # size="2 2"
    "Torus":      (0.55, 0.15, 0.55),  # major=0.4, minor=0.15
    "TorusThin":  (0.45, 0.05, 0.45),  # major=0.4, minor=0.05
    "Capsule":    (0.3,  0.7,  0.3),   # radius=0.3, height=0.8 -> y = 0.8/2+0.3
    "Disk":       (0.5,  0.0,  0.5),   # radius=0.5
    "RingDisk":   (0.5,  0.0,  0.5),   # outer radius=0.5 (inner_radius doesn't change bbox)
    "Grid":       (0.5,  0.0,  0.5),   # size="1 1 1" -> x/z half = 0.5
    "IcoSphere":  (0.5,  0.5,  0.5),   # radius=0.5
}
BBOX_TOLERANCE = 0.02  # generous tolerance for tessellation-approximation shapes (sphere/torus/icosphere)


def test_bbox_matches_scene_renderer_scale_formula(mc3togltf, xml_path, tmpdir):
    out_gltf = os.path.join(tmpdir, "out_bbox_check.gltf")
    r = run([mc3togltf, xml_path, out_gltf])
    assert r.returncode == 0, f"mc3togltf failed:\n{r.stderr}"

    with open(out_gltf) as f:
        gltf = json.load(f)
    accs = gltf.get("accessors", [])

    for name, expected_half in EXPECTED_BBOX_HALF_EXTENT.items():
        _, prim = check_node_has_geometry(gltf, name)
        acc = accs[prim["attributes"]["POSITION"]]
        actual_half = tuple(max(abs(mn), abs(mx)) for mn, mx in zip(acc["min"], acc["max"]))
        for axis, axis_name in enumerate("XYZ"):
            exp_v, act_v = expected_half[axis], actual_half[axis]
            assert abs(exp_v - act_v) <= BBOX_TOLERANCE, (
                f"AUDIT-0012: '{name}' axis {axis_name} bbox half-extent mismatch between "
                f"mc3togltf's buildPrimitive() output ({act_v:.4f}) and the value "
                f"analytically expected from SceneRenderer's own scale formula "
                f"({exp_v:.4f}) -- possible silent divergence between the two "
                f"independent primitive-geometry implementations"
            )

    print("bbox_matches_scene_renderer_scale_formula: PASS")


def test_all_objects(mc3togltf, xml_path, tmpdir):
    if not os.path.exists(xml_path):
        print(f"Skipping all_objects test: {xml_path} not found")
        return

    out_gltf = os.path.join(tmpdir, "out_objects.gltf")
    r = run([mc3togltf, xml_path, out_gltf])
    assert r.returncode == 0, f"mc3togltf all_objects failed:\n{r.stderr}"
    assert os.path.exists(out_gltf) and os.path.getsize(out_gltf) > 0, \
        "all_objects GLTF output is empty"

    with open(out_gltf) as f:
        gltf = json.load(f)

    assert gltf.get("asset", {}).get("version") == "2.0"

    # Verify all expected node names are present
    nmap = node_map(gltf)
    for name in EXPECTED_OBJECTS_NODES:
        assert name in nmap, (
            f"Expected node '{name}' missing.\n"
            f"Present nodes: {sorted(nmap.keys())}"
        )

    # Tetra (mesh/OBJ) and Pipe (extrude) must have geometry
    check_node_has_geometry(gltf, "Tetra")
    check_node_has_geometry(gltf, "Pipe")
    check_node_has_geometry(gltf, "GroupSphere")
    check_node_has_geometry(gltf, "GroupBox")

    # CubeA and CubeB are <instance> nodes from <definitions>: must have geometry.
    # They intentionally use *different* materials (red/blue) to exercise
    # per-instance material override — so, correctly, they do NOT share a
    # glTF mesh index (mesh sharing is keyed on definition+material+deform;
    # see buildDefCacheKey() in GltfExporter.cpp). The same-definition+
    # same-material shared-mesh case (STAB-0198) is already covered by
    # instance_deform_cache_test.py's BlockA/BlockB assertions.
    check_node_has_geometry(gltf, "CubeA")
    check_node_has_geometry(gltf, "CubeB")

    # STAB-0197: <group name="MyGroup"> contains GroupSphere/GroupBox as XML
    # children — the exported node tree must reflect that parent-child
    # relationship via glTF's node.children index array, not just export all
    # three as unrelated top-level nodes.
    nodes = gltf.get("nodes", [])
    my_group = nmap["MyGroup"]
    group_children_idx = my_group.get("children", [])
    assert group_children_idx, "MyGroup node has no children array (hierarchy lost on export)"
    child_names = {nodes[i].get("name") for i in group_children_idx if 0 <= i < len(nodes)}
    assert {"GroupSphere", "GroupBox"} <= child_names, (
        f"MyGroup.children should reference GroupSphere and GroupBox, "
        f"got child node names: {sorted(n for n in child_names if n)}"
    )

    # TriggerZone is an <area>: must exist but no mesh; must have extras.mc3_type=="area"
    tz_node = nmap["TriggerZone"]
    assert tz_node.get("mesh") is None, "TriggerZone is an area and should have no mesh"
    tz_extras = tz_node.get("extras", {})
    assert tz_extras.get("mc3_type") == "area", (
        f"TriggerZone extras.mc3_type should be 'area', got: {tz_extras!r}"
    )

    # At least one camera must be exported
    cameras = gltf.get("cameras", [])
    assert len(cameras) >= 1, f"Expected at least 1 camera, got {len(cameras)}"

    # Lights (directional "Sun") must produce KHR_lights_punctual extension
    ext_used = gltf.get("extensionsUsed", [])
    assert "KHR_lights_punctual" in ext_used, (
        f"Expected KHR_lights_punctual in extensionsUsed, got: {ext_used}"
    )
    ext_data = gltf.get("extensions", {})
    assert "KHR_lights_punctual" in ext_data, "extensions.KHR_lights_punctual missing"

    print("all_objects_export_test: PASS")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <all_primitives.mc3.xml> <all_objects.mc3.xml>")
        sys.exit(1)

    mc3togltf_bin = sys.argv[1]
    prims_xml = sys.argv[2]
    objs_xml  = sys.argv[3]

    with tempfile.TemporaryDirectory() as tmpdir:
        try:
            test_all_primitives(mc3togltf_bin, prims_xml, tmpdir)
            test_all_objects(mc3togltf_bin, objs_xml, tmpdir)
            test_bbox_matches_scene_renderer_scale_formula(mc3togltf_bin, prims_xml, tmpdir)
        except AssertionError as e:
            print(f"FAIL: {e}", file=sys.stderr)
            sys.exit(1)

    print("All export verification tests: PASS")
