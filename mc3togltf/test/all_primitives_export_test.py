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


def test_all_primitives(mc3togltf, xml_path, tmpdir):
    out_gltf = os.path.join(tmpdir, "out_primitives.gltf")
    r = run([mc3togltf, xml_path, out_gltf])
    assert r.returncode == 0, f"mc3togltf failed:\n{r.stderr}"
    assert os.path.exists(out_gltf) and os.path.getsize(out_gltf) > 0, "Output GLTF is empty"

    with open(out_gltf) as f:
        gltf = json.load(f)

    assert gltf.get("asset", {}).get("version") == "2.0", \
        f"asset.version != '2.0', got: {gltf.get('asset', {}).get('version')}"

    node_names = {n.get("name", "") for n in gltf.get("nodes", [])}
    missing = [n for n in EXPECTED_PRIMITIVE_NODES if n not in node_names]
    assert not missing, (
        f"Missing nodes in GLTF output: {missing}\n"
        f"Present nodes: {sorted(node_names)}"
    )

    meshes = gltf.get("meshes", [])
    assert len(meshes) >= len(EXPECTED_PRIMITIVE_NODES), (
        f"Expected at least {len(EXPECTED_PRIMITIVE_NODES)} meshes, got {len(meshes)}"
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

    node_names = {n.get("name", "") for n in gltf.get("nodes", [])}
    assert node_names, "No nodes exported from all_objects"

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
