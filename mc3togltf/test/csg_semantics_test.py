#!/usr/bin/env python3
"""
S5 CSG semantics: STAB-0207/0208/0209/0212/0219/0227/0228/0234.

Uses test/csg_semantics.mc3.xml, which encodes each check as a pair or
single CSG node whose *geometry* (not just presence) proves the exporter
did the right thing:

  - STAB-0208/0234 (isCutter/role="cutter" semantics): WithCutter (real
    subtraction) vs. WithoutCutter (same two children, no cutter flag,
    so Difference treats the sphere as a second *base* child unioned in
    -- and since the sphere is fully inside the box, the union result is
    just the box alone). These must differ drastically.
  - STAB-0207 (child transform composed into world space before the
    boolean op): OffsetCutterDiff (cutter translated off-center, partially
    breaking through the box's +X face) vs. CenteredCutterDiff (same
    radius cutter, no offset, fully enclosed). Must differ -- if the
    exporter dropped the cutter's own local transform, both would be
    identical.
  - STAB-0209/0219 (empty CSG result policy): EmptyIntersection (two
    non-overlapping boxes) and FullHoleDiff (cutter fully engulfing the
    base) must both export successfully (no crash) with a warning
    naming the empty result, and the resulting node must have no mesh.
  - STAB-0212 (material set directly on a CSG node applies to the merged
    result): MaterialOnCsgNode's primitive.material must resolve to
    "cutmat".
  - STAB-0227 (intersection of two identical spheres returns ~the same
    sphere, not empty/degenerate): IdenticalSphereIntersection must have
    a real, non-trivial vertex count.
"""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def node_map(gltf):
    return {n.get("name", ""): n for n in gltf.get("nodes", [])}


def vertex_count(gltf, node_name):
    nmap = node_map(gltf)
    node = nmap[node_name]
    mesh_idx = node.get("mesh")
    if mesh_idx is None:
        return None
    meshes = gltf.get("meshes", [])
    prims = meshes[mesh_idx].get("primitives", [])
    if not prims:
        return None
    pos_idx = prims[0].get("attributes", {}).get("POSITION")
    if pos_idx is None:
        return None
    accs = gltf.get("accessors", [])
    return accs[pos_idx].get("count", 0) if 0 <= pos_idx < len(accs) else None


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <csg_semantics.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, (
            f"Expected export to succeed (empty CSG results warn, not fail), "
            f"got returncode={r.returncode}\nstderr: {r.stderr}"
        )

        combined = r.stdout + r.stderr

        with open(out) as f:
            gltf = json.load(f)

        # STAB-0208/0234: isCutter must genuinely change the result.
        with_cutter    = vertex_count(gltf, "WithCutter")
        without_cutter = vertex_count(gltf, "WithoutCutter")
        assert with_cutter is not None and with_cutter > 0, (
            f"WithCutter should have real cut geometry, got {with_cutter}"
        )
        assert without_cutter is not None and without_cutter > 0, (
            f"WithoutCutter should still export the box (union, sphere fully "
            f"inside), got {without_cutter}"
        )
        assert with_cutter != without_cutter, (
            f"role=\"cutter\" must change the boolean result: WithCutter "
            f"({with_cutter}v, real subtraction) and WithoutCutter "
            f"({without_cutter}v, union — sphere absorbed) must differ, "
            f"but both are {with_cutter}"
        )
        # The cut cavity adds interior wall geometry; the union (sphere fully
        # inside the box, so its surface is discarded entirely) must be just
        # the box's own 12 triangles = 36 verts, far fewer than the real cut.
        assert with_cutter > without_cutter, (
            f"Expected WithCutter ({with_cutter}v) > WithoutCutter "
            f"({without_cutter}v) -- a real cavity should add more geometry "
            f"than a union that fully absorbs the sphere"
        )
        print(f"STAB-0208/0234: isCutter changes the result "
              f"(WithCutter={with_cutter}v, WithoutCutter={without_cutter}v) — PASS")

        # STAB-0207: the cutter child's own local transform must be applied
        # (world-space composition), not ignored.
        offset_verts   = vertex_count(gltf, "OffsetCutterDiff")
        centered_verts = vertex_count(gltf, "CenteredCutterDiff")
        assert offset_verts is not None and offset_verts > 0
        assert centered_verts is not None and centered_verts > 0
        assert offset_verts != centered_verts, (
            f"OffsetCutterDiff ({offset_verts}v) and CenteredCutterDiff "
            f"({centered_verts}v) must differ -- if the cutter's own position "
            f"were ignored, an off-center and a centered cutter would produce "
            f"identical geometry"
        )
        print(f"STAB-0207: cutter child's local transform is applied "
              f"(offset={offset_verts}v, centered={centered_verts}v) — PASS")

        # STAB-0209/0219: empty CSG result -- no crash, warning, no mesh.
        for empty_name in ("EmptyIntersection", "FullHoleDiff"):
            nmap = node_map(gltf)
            assert empty_name in nmap, f"Expected node '{empty_name}' in output"
            assert nmap[empty_name].get("mesh") is None, (
                f"{empty_name}: expected no mesh for an empty CSG result, "
                f"got mesh={nmap[empty_name].get('mesh')}"
            )
        assert "empty volume" in combined, (
            f"Expected a warning naming the empty CSG result, got:\n{combined}"
        )
        assert combined.count("empty volume") >= 2, (
            f"Expected an empty-volume warning for both EmptyIntersection and "
            f"FullHoleDiff, got:\n{combined}"
        )
        print("STAB-0209/0219: empty CSG result (non-overlapping intersection, "
              "full-hole difference) — no mesh, warning printed, no crash — PASS")

        # STAB-0212: material set directly on the CSG node applies to the
        # merged result mesh.
        nmap = node_map(gltf)
        mat_node = nmap["MaterialOnCsgNode"]
        mesh_idx = mat_node.get("mesh")
        assert mesh_idx is not None, "MaterialOnCsgNode has no mesh"
        prim = gltf["meshes"][mesh_idx]["primitives"][0]
        mat_idx = prim.get("material")
        assert mat_idx is not None, "MaterialOnCsgNode's primitive has no material"
        materials = gltf.get("materials", [])
        assert 0 <= mat_idx < len(materials) and materials[mat_idx].get("name") == "cutmat", (
            f"Expected MaterialOnCsgNode's material to resolve to 'cutmat', "
            f"got: {materials[mat_idx] if 0 <= mat_idx < len(materials) else None}"
        )
        print("STAB-0212: material set on the CSG node itself applies to the "
              "merged result — PASS")

        # STAB-0227: intersection of two identical spheres returns a real,
        # non-degenerate sphere back (not empty, not near-zero).
        identical_verts = vertex_count(gltf, "IdenticalSphereIntersection")
        assert identical_verts is not None and identical_verts > 200, (
            f"Expected a real, non-degenerate sphere from intersecting two "
            f"identical spheres, got {identical_verts} vertices"
        )
        print(f"STAB-0227: identical-sphere intersection returns real geometry "
              f"({identical_verts}v) — PASS")

    print("\nCSG semantics test: PASS")
