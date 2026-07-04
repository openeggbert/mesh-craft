#!/usr/bin/env python3
"""
STAB-0166/0167/0168/0169/0170 (S2/S3) + STAB-0411/0412/0413/0414/0415 (S11):
material PBR fields must export to the correct glTF material properties.

  - base_color        -> pbrMetallicRoughness.baseColorFactor (all 4 components,
                          including alpha)
  - metallic          -> pbrMetallicRoughness.metallicFactor
  - roughness         -> pbrMetallicRoughness.roughnessFactor
  - emissive_color     -> emissiveFactor
  - alpha_mode="blend" -> alphaMode="BLEND"
  - alpha_mode="mask" + alpha_cutoff -> alphaMode="MASK" + alphaCutoff
"""
import json
import os
import subprocess
import sys
import tempfile

TOL = 1e-5

EXPECTED = {
    "mat_metal_rough": {
        "baseColorFactor": [0.6, 0.6, 0.6, 1.0],
        "metallicFactor": 0.8,
        "roughnessFactor": 0.3,
    },
    "mat_emissive": {
        "baseColorFactor": [0.0, 0.0, 0.0, 1.0],
        "emissiveFactor": [1.0, 0.5, 0.0],
    },
    "mat_blend": {
        "baseColorFactor": [0.2, 0.4, 0.8, 0.5],
        "alphaMode": "BLEND",
    },
    "mat_mask": {
        "baseColorFactor": [1.0, 1.0, 1.0, 0.9],
        "alphaMode": "MASK",
        "alphaCutoff": 0.7,
    },
}


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def close(a, b):
    return abs(a - b) < TOL


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <material_pbr.mc3.xml>")
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

        materials = gltf.get("materials", [])
        mat_by_name = {m.get("name", ""): m for m in materials}

        for mat_id, expected in EXPECTED.items():
            assert mat_id in mat_by_name, (
                f"Expected material '{mat_id}' in output, got: {sorted(mat_by_name.keys())}"
            )
            mat = mat_by_name[mat_id]
            pbr = mat.get("pbrMetallicRoughness", {})

            if "baseColorFactor" in expected:
                actual = pbr.get("baseColorFactor")
                assert actual is not None and len(actual) == 4 and all(
                    close(a, b) for a, b in zip(actual, expected["baseColorFactor"])
                ), f"{mat_id}.baseColorFactor: expected {expected['baseColorFactor']}, got {actual}"

            if "metallicFactor" in expected:
                assert close(pbr.get("metallicFactor", -1), expected["metallicFactor"]), (
                    f"{mat_id}.metallicFactor: expected {expected['metallicFactor']}, got {pbr.get('metallicFactor')}"
                )

            if "roughnessFactor" in expected:
                assert close(pbr.get("roughnessFactor", -1), expected["roughnessFactor"]), (
                    f"{mat_id}.roughnessFactor: expected {expected['roughnessFactor']}, got {pbr.get('roughnessFactor')}"
                )

            if "emissiveFactor" in expected:
                actual = mat.get("emissiveFactor")
                assert actual is not None and len(actual) == 3 and all(
                    close(a, b) for a, b in zip(actual, expected["emissiveFactor"])
                ), f"{mat_id}.emissiveFactor: expected {expected['emissiveFactor']}, got {actual}"

            if "alphaMode" in expected:
                assert mat.get("alphaMode") == expected["alphaMode"], (
                    f"{mat_id}.alphaMode: expected {expected['alphaMode']}, got {mat.get('alphaMode')}"
                )

            if "alphaCutoff" in expected:
                assert close(mat.get("alphaCutoff", -1), expected["alphaCutoff"]), (
                    f"{mat_id}.alphaCutoff: expected {expected['alphaCutoff']}, got {mat.get('alphaCutoff')}"
                )

            print(f"{mat_id}: {expected} — PASS")

    print("\nMaterial PBR export test: PASS")
