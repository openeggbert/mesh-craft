#!/usr/bin/env python3
"""
STAB-0247/0248: definition/instance edge cases.

  - STAB-0247: an <instance> of a definition whose root is a <group> with
    zero children must export without crashing (exit 0).
  - STAB-0248: a definition with multiple children (a group of 2 objects),
    instanced 10 times, must produce 10 real instance node trees, with the
    underlying meshes shared (not duplicated 10x) across all instances.
"""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def test_empty_group_definition(mc3togltf, xml_path):
    out_dir = tempfile.mkdtemp()
    out = os.path.join(out_dir, "out.gltf")
    r = run([mc3togltf, xml_path, out])
    assert r.returncode == 0, (
        f"STAB-0247: expected export to succeed for an empty-group definition "
        f"instance, got returncode={r.returncode}\nstderr: {r.stderr}"
    )
    with open(out) as f:
        gltf = json.load(f)
    nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}
    assert "EmptyInstance" in nmap, f"Expected node 'EmptyInstance', got: {sorted(nmap.keys())}"
    print("STAB-0247: instance of an empty-group definition exports without crash — PASS")


def test_multi_child_definition(mc3togltf, xml_path):
    out_dir = tempfile.mkdtemp()
    out = os.path.join(out_dir, "out.gltf")
    r = run([mc3togltf, xml_path, out])
    assert r.returncode == 0, (
        f"STAB-0248: multi-child definition instancing export failed "
        f"(returncode={r.returncode}):\n{r.stderr}"
    )
    with open(out) as f:
        gltf = json.load(f)

    nodes  = gltf.get("nodes", [])
    meshes = gltf.get("meshes", [])
    nmap = {n.get("name", ""): n for n in nodes}

    instance_names = [f"Inst{i}" for i in range(10)]
    for name in instance_names:
        assert name in nmap, f"Expected instance node '{name}', got: {sorted(nmap.keys())}"
        children_idx = nmap[name].get("children", [])
        assert len(children_idx) == 2, (
            f"Expected '{name}' to have 2 children (PairBox, PairSphere), got {len(children_idx)}"
        )
        child_names = {nodes[ci].get("name") for ci in children_idx}
        assert child_names == {"PairBox", "PairSphere"}, (
            f"Expected '{name}' children to be PairBox/PairSphere, got {child_names}"
        )

    # Mesh reuse: only 2 unique meshes (one per definition child) despite 10 instances.
    assert len(meshes) == 2, (
        f"Expected exactly 2 unique meshes (PairBox + PairSphere shapes) shared "
        f"across all 10 instances, got {len(meshes)}"
    )

    # Every instance's PairBox child must reference the SAME mesh index (and
    # likewise for PairSphere) -- real sharing, not 10 separate copies.
    box_mesh_indices = set()
    sphere_mesh_indices = set()
    for name in instance_names:
        for ci in nmap[name].get("children", []):
            child = nodes[ci]
            if child.get("name") == "PairBox":
                box_mesh_indices.add(child.get("mesh"))
            elif child.get("name") == "PairSphere":
                sphere_mesh_indices.add(child.get("mesh"))
    assert len(box_mesh_indices) == 1, f"Expected all PairBox children to share 1 mesh index, got {box_mesh_indices}"
    assert len(sphere_mesh_indices) == 1, f"Expected all PairSphere children to share 1 mesh index, got {sphere_mesh_indices}"

    print(f"STAB-0248: 10 instances of a 2-child definition share exactly "
          f"2 meshes ({len(nodes)} nodes total) — PASS")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <empty_definition.mc3.xml> <multi_child_definition.mc3.xml>")
        sys.exit(1)

    mc3togltf     = sys.argv[1]
    empty_def_xml = sys.argv[2]
    multi_def_xml = sys.argv[3]

    try:
        test_empty_group_definition(mc3togltf, empty_def_xml)
        test_multi_child_definition(mc3togltf, multi_def_xml)
    except AssertionError as e:
        print(f"FAIL: {e}", file=sys.stderr)
        sys.exit(1)

    print("\nDefinition instancing edge-case test: PASS")
