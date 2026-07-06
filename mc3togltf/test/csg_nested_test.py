#!/usr/bin/env python3
"""
STAB-0205/0206: Verify nested CSG exports correctly, 3 levels deep
(Difference -> Union -> Union).

A prior version of this test's fixture omitted role="cutter" on the
top-level cutter child, so Difference silently treated it as a second
*base* child (unioned in) instead of subtracting it -- since the cutter
was fully inside the base box, the "union" degenerated to just the box
alone (36 vertices) and this test's old assertions (vertex count > 0,
index count > 0) passed anyway without ever proving a real cut happened.
Fixed by adding role="cutter" and asserting a vertex count high enough
that it could only come from a genuinely hollowed-out result.
"""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def accessor_count(gltf, idx):
    accs = gltf.get("accessors", [])
    return accs[idx].get("count", 0) if 0 <= idx < len(accs) else 0


def test_nested_csg(mc3togltf, xml_path, tmpdir):
    out = os.path.join(tmpdir, "nested.gltf")
    r = run([mc3togltf, xml_path, out])
    assert r.returncode == 0, (
        f"Nested CSG export failed (returncode={r.returncode}):\n{r.stderr}"
    )

    with open(out) as f:
        gltf = json.load(f)

    nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}

    # Root CSG node must be present
    assert "NestedCsg" in nmap, (
        f"CSG root 'NestedCsg' missing. Present: {sorted(nmap.keys())}"
    )
    node = nmap["NestedCsg"]

    # Must have a mesh (boolean result baked in)
    mesh_idx = node.get("mesh")
    assert mesh_idx is not None, "NestedCsg has no mesh — boolean result not exported"

    prims = gltf["meshes"][mesh_idx].get("primitives", [])
    assert len(prims) > 0, "NestedCsg mesh has no primitives"

    pos_idx = prims[0].get("attributes", {}).get("POSITION")
    assert pos_idx is not None, "NestedCsg primitive missing POSITION accessor"
    vc = accessor_count(gltf, pos_idx)
    assert vc > 0, f"NestedCsg has 0 vertices after CSG eval"

    # STAB-0206: prove a *real* 3-level cut happened, not a silent no-op.
    # BoxBase alone is 12 triangles = 36 vertices; a genuinely hollowed-out
    # box (3 cavities carved via a 3-level Difference->Union->Union cutter
    # tree) must have far more geometry than that.
    assert vc > 1000, (
        f"NestedCsg has only {vc} vertices -- expected a real hollowed-out "
        f"result (thousands of vertices from 3 carved cavities), not a "
        f"near-bare box (36v would mean the cutter was silently ignored, "
        f"e.g. a missing role=\"cutter\" flag)"
    )

    idx_acc = prims[0].get("indices")
    assert idx_acc is not None, "NestedCsg primitive missing indices accessor"
    ic = accessor_count(gltf, idx_acc)
    assert ic > 0,      f"NestedCsg has 0 indices (zero triangles)"
    assert ic % 3 == 0, f"NestedCsg index count {ic} not a multiple of 3"

    # All child nodes across all 3 levels must be fully baked — they must
    # NOT appear as separate glTF nodes.
    baked_children = ["BoxBase", "HolePair", "HoleA", "HoleBGroup", "HoleB1", "HoleB2"]
    for child in baked_children:
        assert child not in nmap, (
            f"Child '{child}' should be baked into NestedCsg, "
            f"but appears as a separate glTF node"
        )

    print(f"csg_nested_test: PASS (verts={vc}, indices={ic})")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <csg_nested.mc3.xml>")
        sys.exit(1)

    with tempfile.TemporaryDirectory() as tmpdir:
        try:
            test_nested_csg(sys.argv[1], sys.argv[2], tmpdir)
        except AssertionError as e:
            print(f"FAIL: {e}", file=sys.stderr)
            sys.exit(1)
