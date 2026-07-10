#!/usr/bin/env python3
"""
STAB-0698: glTF core spec has no scene-background/skybox/fog concept, so
mc3togltf preserves Mc3Environment via scene.extras -- but skyboxTexture
(equirectangular panorama) was missing from applyEnvironment(), unlike
its sibling backgroundTexture. No prior test covered scene.extras'
environment data at all.

Usage: environment_export_test.py <mc3togltf-binary>
"""
import json
import os
import subprocess
import sys
import tempfile

failures = 0

def check(cond, msg):
    global failures
    if cond:
        print(f"PASS: {msg}")
    else:
        print(f"FAIL: {msg}", file=sys.stderr)
        failures += 1

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf-binary>", file=sys.stderr)
        sys.exit(2)
    mc3togltf = sys.argv[1]

    fixture = """<?xml version="1.0" encoding="UTF-8"?>
<mc3 version="0.3" model="EnvironmentExportTest">
  <environment>
    <background color="0.1 0.2 0.3"/>
    <background_texture>textures/bg.png</background_texture>
    <skybox_texture>textures/sky_panorama.hdr</skybox_texture>
    <fog color="0.5 0.6 0.7" mode="exponential" start="5" end="50" density="0.02"/>
  </environment>
  <objects>
    <box name="Box" size="1 1 1"/>
  </objects>
</mc3>
"""
    with tempfile.TemporaryDirectory() as tmpdir:
        xml_path = os.path.join(tmpdir, "env.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(fixture)
        out_gltf = os.path.join(tmpdir, "out.gltf")

        r = subprocess.run([mc3togltf, xml_path, out_gltf], capture_output=True, text=True)
        check(r.returncode == 0, f"exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            sys.exit(1)

        with open(out_gltf) as f:
            g = json.load(f)

        extras = g.get("scenes", [{}])[g.get("scene", 0)].get("extras", {})
        check(extras.get("backgroundTexture") == "textures/bg.png",
              "scene.extras.backgroundTexture present and correct")
        check(extras.get("skyboxTexture") == "textures/sky_panorama.hdr",
              "scene.extras.skyboxTexture present and correct (was missing, STAB-0698)")
        check("fog" in extras and extras["fog"].get("mode") == "exponential",
              "scene.extras.fog present with correct mode")

    if failures:
        print(f"\n{failures} FAILURE(S)", file=sys.stderr)
        sys.exit(1)
    print("\nAll environment export checks passed.")

if __name__ == "__main__":
    main()
