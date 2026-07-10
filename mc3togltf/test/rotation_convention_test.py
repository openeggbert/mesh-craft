#!/usr/bin/env python3
"""
STAB-0691: doc.rotationUnits/doc.eulerOrder were completely ignored by
GltfExporter.cpp (hardcoded degrees + extrinsic XYZ composition via
eulerXYZToQuat()) -- any document declaring rotation_units="radians" or
a non-default euler_order exported systematically wrong rotations for
every rotated object/camera, silently, with no error.

Verifies against an INDEPENDENT reference (scipy's Rotation, not this
project's own code in either direction) rather than cross-checking
against the editor: a parallel investigation while implementing this
fix found the editor's own SceneRenderer.cpp (computeObjWorldMatrix())
ALSO hardcodes degrees + a fixed XNA-internal rotation order and does
not consult these fields either -- so the editor cannot serve as a
"known-correct" reference for this specific behavior (see plan.md
STAB-0701, filed separately, for that broader finding).

Usage: rotation_convention_test.py <mc3togltf-binary> <rotation_convention_export.mc3.xml>
"""
import json
import math
import os
import subprocess
import sys
import tempfile

try:
    from scipy.spatial.transform import Rotation
except ImportError:
    print("SKIP: scipy not available, cannot compute an independent reference quaternion", file=sys.stderr)
    sys.exit(0)

failures = 0

def check(cond, msg):
    global failures
    if cond:
        print(f"PASS: {msg}")
    else:
        print(f"FAIL: {msg}", file=sys.stderr)
        failures += 1

def quat_close(a, b, tol=1e-4):
    """Quaternions q and -q represent the same rotation."""
    d_pos = max(abs(x - y) for x, y in zip(a, b))
    d_neg = max(abs(x + y) for x, y in zip(a, b))
    return min(d_pos, d_neg) < tol

def reference_quat(x, y, z, order):
    """x/y/z are the rotation angles AROUND the X/Y/Z axes respectively
    (mc3's <box rotation="x y z">). order e.g. "ZYX" says the sequence in
    which those per-axis rotations are composed: mc3togltf's eulerToQuat()
    treats order[0] as first/innermost and order[2] as last/outermost
    (R = R[order[2]] * R[order[1]] * R[order[0]]).

    Empirically cross-checked (not just derived): scipy's Rotation.from_euler
    with an UPPERCASE (extrinsic) sequence string applies its angles array in
    the OPPOSITE chronological sense from that -- from_euler(S, angles)
    ends up equal to mc3togltf's eulerToQuat(..., order=reversed(S)) when
    angles is built by walking reversed(S). Confirmed for both "XYZ" and
    "ZYX" against mc3togltf's own (independently-validated-against-the-
    pre-existing eulerXYZToQuat) output before trusting this as a reference."""
    rev_order = order[::-1]
    angle_by_axis = {'X': x, 'Y': y, 'Z': z}
    seq_angles = [angle_by_axis[c] for c in rev_order]
    r = Rotation.from_euler(rev_order, seq_angles, degrees=False)
    qx, qy, qz, qw = r.as_quat()
    return (qx, qy, qz, qw)

def main():
    if len(sys.argv) != 3:
        print("usage: rotation_convention_test.py <mc3togltf-binary> <fixture.mc3.xml>", file=sys.stderr)
        sys.exit(2)
    binary, fixture = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".gltf", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, fixture, out], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            return

        with open(out) as f:
            g = json.load(f)

        # euler_order="ZYX", rotation="0.3 0.7 -0.5" (radians) on RotatedBox.
        box_node = next((n for n in g["nodes"] if n.get("name") == "RotatedBox"), None)
        check(box_node is not None, "RotatedBox node present")
        if box_node:
            expected = reference_quat(0.3, 0.7, -0.5, "ZYX")
            actual = tuple(box_node.get("rotation", [0, 0, 0, 1]))
            check(quat_close(actual, expected),
                  f"RotatedBox quaternion matches independent ZYX-radians reference "
                  f"(expected {expected}, got {actual})")
            # Sanity: must NOT match the old (buggy) behavior, which hardcoded
            # XYZ order and treated the raw radian values (0.3, 0.7, -0.5) as
            # if they were DEGREES -- a tiny, wildly different rotation.
            wrong = Rotation.from_euler("XYZ", [0.3, 0.7, -0.5], degrees=True).as_quat()
            wrong = (wrong[0], wrong[1], wrong[2], wrong[3])
            check(not quat_close(actual, wrong, tol=1e-3),
                  "RotatedBox quaternion is NOT the old buggy XYZ-degrees-misread value")

        cam_node = next((n for n in g["nodes"] if n.get("name") == "Main"), None)
        check(cam_node is not None, "camera node present")
        if cam_node:
            expected = reference_quat(0.1, 0.2, -0.15, "ZYX")
            actual = tuple(cam_node.get("rotation", [0, 0, 0, 1]))
            check(quat_close(actual, expected),
                  f"camera quaternion matches independent ZYX-radians reference "
                  f"(expected {expected}, got {actual})")

            # STAB-0694: aspectRatio was hardcoded to 16:9 for every camera.
            cam_idx = cam_node.get("camera")
            if cam_idx is not None:
                gcam = g["cameras"][cam_idx]["perspective"]
                check("aspectRatio" not in gcam,
                      "camera perspective.aspectRatio is omitted, not hardcoded to 16:9 "
                      "(tinygltf only serializes it when explicitly set > 0)")

    finally:
        if os.path.exists(out):
            os.unlink(out)

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll rotation convention checks passed.")

if __name__ == "__main__":
    main()
