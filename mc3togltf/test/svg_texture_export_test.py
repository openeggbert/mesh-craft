#!/usr/bin/env python3
"""SVG material textures are rasterized and retained in exported glTF."""
import json
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <external-svg-scene> <inline-svg-scene>")
        sys.exit(1)

    mc3togltf, xml_paths = sys.argv[1], sys.argv[2:]

    with tempfile.TemporaryDirectory() as tmpdir:
        for i, xml_path in enumerate(xml_paths):
            out = os.path.join(tmpdir, f"out_{i}.gltf")
            r = run([mc3togltf, xml_path, out])
            assert r.returncode == 0, f"Export failed for {xml_path}:\n{r.stderr}"
            assert os.path.exists(out) and os.path.getsize(out) > 0, \
                "Output glTF is missing or empty"

            with open(out, encoding="utf-8") as f:
                gltf = json.load(f)
            material = gltf["materials"][0]
            texture_index = material["pbrMetallicRoughness"]["baseColorTexture"]["index"]
            assert 0 <= texture_index < len(gltf["textures"]), "SVG texture missing from material"
            image = gltf["images"][gltf["textures"][texture_index]["source"]]
            assert image.get("uri", "").endswith(".png"), f"Raster PNG missing: {image}"
            generated_png = os.path.join(tmpdir, image["uri"])
            assert os.path.exists(generated_png) and os.path.getsize(generated_png) > 0, \
                "Rasterized SVG PNG was not written"
            assert "SVG rasterization is not implemented" not in (r.stdout + r.stderr)
        print("SVG texture export: PASS (external and inline)")

    print("\nSVG texture export test: PASS")
