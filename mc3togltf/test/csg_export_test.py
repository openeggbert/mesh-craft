#!/usr/bin/env python3
"""
Verify that CSG export (union/difference/intersection) produces real geometry.

Checks:
- Export succeeds without --allow-approximate-csg.
- The CSG root node has a mesh (boolean result), not just children.
- The CSG mesh has non-zero vertex count.
- Child nodes are NOT exported separately (they are baked into the boolean result).
"""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def check_csg_result(gltf, root_name, expected_child_names=()):
    """Verify that root_name has a mesh with geometry, and children are fully baked in."""
    nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}
    assert root_name in nmap, (
        f"CSG root '{root_name}' missing from output.\n"
        f"Present nodes: {sorted(nmap.keys())}"
    )
    node = nmap[root_name]
    mesh_idx = node.get("mesh")
    assert mesh_idx is not None, (
        f"CSG root '{root_name}' has no mesh — boolean result was not exported"
    )
    meshes = gltf.get("meshes", [])
    assert 0 <= mesh_idx < len(meshes), f"mesh_idx {mesh_idx} out of range"
    prims = meshes[mesh_idx].get("primitives", [])
    assert len(prims) > 0, f"CSG root '{root_name}' mesh has no primitives"
    pos_acc_idx = prims[0].get("attributes", {}).get("POSITION")
    assert pos_acc_idx is not None, f"CSG root '{root_name}' primitive has no POSITION"
    accs = gltf.get("accessors", [])
    vc = accs[pos_acc_idx].get("count", 0) if 0 <= pos_acc_idx < len(accs) else 0
    assert vc > 0, f"CSG root '{root_name}' has 0 vertices"

    # Child nodes must NOT appear anywhere in the glTF node list —
    # they are fully baked into the CSG boolean mesh.
    all_node_names = {n.get("name", "") for n in gltf.get("nodes", [])}
    for child_name in expected_child_names:
        assert child_name not in all_node_names, (
            f"CSG child '{child_name}' appears as a glTF node after real CSG export of "
            f"'{root_name}' — it should be fully baked into the boolean result mesh."
        )

    return vc


def test_union(mc3togltf, xml_path, tmpdir):
    out = os.path.join(tmpdir, "union.gltf")
    r = run([mc3togltf, xml_path, out])
    assert r.returncode == 0, (
        f"Union export failed (returncode={r.returncode}):\n{r.stderr}"
    )
    with open(out) as f:
        gltf = json.load(f)
    vc = check_csg_result(gltf, "MergedBoxes", ("BoxA", "BoxB"))
    # Two overlapping 1×1×1 boxes shifted ±0.3 on X:
    # union volume < 2 full boxes → expect many vertices but not empty
    assert vc > 0, f"Union result has {vc} vertices"
    print(f"union export: PASS (vertices={vc})")


def test_difference(mc3togltf, xml_path, tmpdir):
    out = os.path.join(tmpdir, "diff.gltf")
    r = run([mc3togltf, xml_path, out])
    assert r.returncode == 0, (
        f"Difference export failed (returncode={r.returncode}):\n{r.stderr}"
    )
    with open(out) as f:
        gltf = json.load(f)
    vc = check_csg_result(gltf, "HollowBlock", ("Block", "Cavity"))
    assert vc > 0, f"Difference result has {vc} vertices"
    print(f"difference export: PASS (vertices={vc})")


def test_intersection(mc3togltf, xml_path, tmpdir):
    out = os.path.join(tmpdir, "isect.gltf")
    r = run([mc3togltf, xml_path, out])
    assert r.returncode == 0, (
        f"Intersection export failed (returncode={r.returncode}):\n{r.stderr}"
    )
    with open(out) as f:
        gltf = json.load(f)
    vc = check_csg_result(gltf, "RoundedSolid", ("Block", "Ball"))
    assert vc > 0, f"Intersection result has {vc} vertices"
    print(f"intersection export: PASS (vertices={vc})")


if __name__ == "__main__":
    if len(sys.argv) < 5:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <csg_union.mc3.xml> "
              f"<csg_diff.mc3.xml> <csg_isect.mc3.xml>")
        sys.exit(1)

    mc3togltf_bin = sys.argv[1]
    union_xml = sys.argv[2]
    diff_xml  = sys.argv[3]
    isect_xml = sys.argv[4]

    with tempfile.TemporaryDirectory() as tmpdir:
        try:
            test_union       (mc3togltf_bin, union_xml, tmpdir)
            test_difference  (mc3togltf_bin, diff_xml,  tmpdir)
            test_intersection(mc3togltf_bin, isect_xml, tmpdir)
        except AssertionError as e:
            print(f"FAIL: {e}", file=sys.stderr)
            sys.exit(1)

    print("All CSG export tests: PASS")
