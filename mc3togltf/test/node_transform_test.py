#!/usr/bin/env python3
"""
STAB-0173/0174/0175: static object transform (position/rotation/scale)
must export to the correct glTF node.translation/node.rotation/node.scale.

  - position="1 2 3"   -> node.translation == [1, 2, 3]
  - rotation="0 90 0"  -> node.rotation == the quaternion for a 90-degree
                          yaw (Y-axis) rotation, per GltfExporter's
                          eulerXYZToQuat() extrinsic-XYZ convention
                          (rotation.y feeds the "yaw" parameter):
                          [0, sin(45deg), 0, cos(45deg)]
  - scale="2 2 2"      -> node.scale == [2, 2, 2]

A 4th, untransformed object confirms the *absence* of position/rotation/
scale attributes correctly omits those fields from the glTF node entirely
(GltfExporter only writes non-default transform fields).
"""
import json
import math
import os
import subprocess
import sys
import tempfile

TOL = 1e-5


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def close_vec(a, b, tol=TOL):
    return a is not None and len(a) == len(b) and all(abs(x - y) < tol for x, y in zip(a, b))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <node_transform.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, f"Export failed (returncode={r.returncode}):\n{r.stderr}"
        assert os.path.exists(out) and os.path.getsize(out) > 0, "Output glTF is missing or empty"

        with open(out) as f:
            gltf = json.load(f)

        nmap = {n.get("name", ""): n for n in gltf.get("nodes", [])}

        # STAB-0173: position -> node.translation
        assert "Translated" in nmap, f"Expected node 'Translated', got: {sorted(nmap.keys())}"
        translation = nmap["Translated"].get("translation")
        assert close_vec(translation, [1.0, 2.0, 3.0]), (
            f"Translated.translation: expected [1,2,3], got {translation}"
        )
        print(f"Translated: translation={translation} — PASS")

        # STAB-0174: rotation="0 90 0" (degrees, Y axis) -> quaternion
        assert "Rotated" in nmap, f"Expected node 'Rotated', got: {sorted(nmap.keys())}"
        rotation = nmap["Rotated"].get("rotation")
        half = math.radians(90.0) / 2.0
        expected_q = [0.0, math.sin(half), 0.0, math.cos(half)]
        assert close_vec(rotation, expected_q), (
            f"Rotated.rotation: expected {expected_q} (90deg about Y), got {rotation}"
        )
        # Quaternion must also be normalized (unit length), not just componentwise close.
        norm = math.sqrt(sum(c * c for c in rotation))
        assert abs(norm - 1.0) < TOL, f"Rotated.rotation is not a unit quaternion: norm={norm}"
        print(f"Rotated: rotation={rotation} — PASS")

        # STAB-0175: scale="2 2 2" -> node.scale
        assert "Scaled" in nmap, f"Expected node 'Scaled', got: {sorted(nmap.keys())}"
        scale = nmap["Scaled"].get("scale")
        assert close_vec(scale, [2.0, 2.0, 2.0]), (
            f"Scaled.scale: expected [2,2,2], got {scale}"
        )
        print(f"Scaled: scale={scale} — PASS")

        # Untransformed object: default TRS fields must be omitted entirely,
        # not written out as explicit identity values.
        assert "Identity" in nmap, f"Expected node 'Identity', got: {sorted(nmap.keys())}"
        identity = nmap["Identity"]
        assert "translation" not in identity, f"Identity node should omit default translation, got {identity}"
        assert "rotation" not in identity, f"Identity node should omit default rotation, got {identity}"
        assert "scale" not in identity, f"Identity node should omit default scale, got {identity}"
        print("Identity: no default TRS fields written — PASS")

    print("\nNode transform (position/rotation/scale) export test: PASS")
