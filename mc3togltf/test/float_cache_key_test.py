#!/usr/bin/env python3
"""
Float precision regression test for geometry cache keys.

Two boxes differ only in the 7th significant digit of the X dimension:
  NearlyEqual1: size="1.0000010 1 1"
  NearlyEqual2: size="1.0000020 1 1"

With default ostream precision (6 significant digits) both floats would be
serialised as "1", causing a false cache hit and mesh sharing.

With std::setprecision(std::numeric_limits<float>::max_digits10) == 9, the
values are distinct and the two boxes must use separate glTF meshes.
"""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <close_dimensions.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "close.gltf")
        r   = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, (
            f"Export failed (returncode={r.returncode}):\n{r.stderr}"
        )
        assert os.path.exists(out) and os.path.getsize(out) > 0, \
            "Output glTF is missing or empty"

        with open(out) as f:
            gltf = json.load(f)

        nodes  = gltf.get("nodes",  [])
        meshes = gltf.get("meshes", [])
        nmap   = {n.get("name", ""): n for n in nodes}

        assert "NearlyEqual1" in nmap, "Expected node 'NearlyEqual1'"
        assert "NearlyEqual2" in nmap, "Expected node 'NearlyEqual2'"

        m1 = nmap["NearlyEqual1"].get("mesh", -1)
        m2 = nmap["NearlyEqual2"].get("mesh", -1)
        assert m1 >= 0, "NearlyEqual1 has no mesh"
        assert m2 >= 0, "NearlyEqual2 has no mesh"

        assert m1 != m2, (
            f"NearlyEqual1 (mesh {m1}) and NearlyEqual2 (mesh {m2}) must NOT "
            f"share a glTF mesh — cache key float precision is too low"
        )
        print(f"NearlyEqual1 uses mesh {m1}, NearlyEqual2 uses mesh {m2} — PASS")

        assert len(meshes) == 2, (
            f"Expected exactly 2 unique meshes (one per distinct box), "
            f"got {len(meshes)}"
        )
        print(f"Unique mesh count: {len(meshes)} — PASS")

    print("\nFloat cache key precision test: PASS")
