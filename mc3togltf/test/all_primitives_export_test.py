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
    "Torus", "Capsule", "Disk", "RingDisk", "Grid", "IcoSphere",
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

    # GLB magic-byte check
    out_glb = os.path.join(tmpdir, "out_primitives.glb")
    r = run([mc3togltf, xml_path, out_glb])
    assert r.returncode == 0, f"mc3togltf GLB failed:\n{r.stderr}"
    with open(out_glb, "rb") as f:
        magic = f.read(4)
    assert magic == b"glTF", f"GLB magic mismatch: {magic!r}"

    print("all_primitives_export_test: PASS")


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

    # CubeA and CubeB are <instance> nodes from <definitions>: must have geometry
    check_node_has_geometry(gltf, "CubeA")
    check_node_has_geometry(gltf, "CubeB")

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
        except AssertionError as e:
            print(f"FAIL: {e}", file=sys.stderr)
            sys.exit(1)

    print("All export verification tests: PASS")
