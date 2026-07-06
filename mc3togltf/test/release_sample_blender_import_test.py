#!/usr/bin/env python3
"""
STAB-0642: verify a real, richly-authored release-representative scene
(test/medieval_castle.mc3.xml -- the most complex hand-authored fixture in
the repo: CSG, textures, materials, bloom-triggering emissive content)
survives export -> real headless Blender import cleanly.

Unlike mc3togltf_blender_import (STAB-0252, a synthetically generated
200-object scene) and mc3togltf_material_pbr_blender_import (STAB-0431, one
box + one material), this exercises actual authored release content through
the same pipeline, closing the "no Blender available" gap now that a real
`blender` binary is present in this environment.

Only registered as a ctest when a `blender` executable is found at configure
time (find_program) -- same optional-tooling gate as the other Blender tests.
"""
import json
import os
import subprocess
import sys


BLENDER_HELPER_SCRIPT = """
import bpy
import sys
import json

glb_path = sys.argv[sys.argv.index("--") + 1]
report_path = sys.argv[sys.argv.index("--") + 2]

bpy.ops.wm.read_factory_settings(use_empty=True)
result = bpy.ops.import_scene.gltf(filepath=glb_path)

objects = bpy.context.scene.objects
mesh_objects = [o for o in objects if o.type == 'MESH']

report = {
    "result": list(result),
    "total_objects": len(objects),
    "mesh_objects": len(mesh_objects),
}
with open(report_path, "w") as f:
    json.dump(report, f)
"""


def run(cmd, timeout=90):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def main():
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <blender> <medieval_castle.mc3.xml>")
        sys.exit(1)

    mc3togltf, blender, scene_xml = sys.argv[1], sys.argv[2], sys.argv[3]

    tmpdir = os.path.dirname(scene_xml) or "."
    glb_path = os.path.join(tmpdir, "_release_sample_test.glb")
    helper_path = os.path.join(tmpdir, "_release_sample_blender_helper.py")
    report_path = os.path.join(tmpdir, "_release_sample_report.json")

    try:
        r = run([mc3togltf, scene_xml, glb_path])
        assert r.returncode == 0, f"Export failed (returncode={r.returncode}):\n{r.stderr}"
        assert os.path.exists(glb_path) and os.path.getsize(glb_path) > 0, \
            "Export produced no (or empty) GLB file"

        with open(helper_path, "w") as f:
            f.write(BLENDER_HELPER_SCRIPT)

        r = run([blender, "--background", "--python", helper_path, "--", glb_path, report_path])
        assert r.returncode == 0, (
            f"Blender import script failed (returncode={r.returncode}):\n"
            f"stdout={r.stdout}\nstderr={r.stderr}"
        )
        assert os.path.exists(report_path), (
            f"Blender did not produce a report file -- likely crashed or the "
            f"import script errored before writing it.\nstdout={r.stdout}\nstderr={r.stderr}"
        )

        with open(report_path) as f:
            report = json.load(f)

        assert report["result"] == ["FINISHED"], (
            f"Expected Blender's glTF import operator to report FINISHED, got {report['result']}"
        )
        assert report["mesh_objects"] > 10, (
            f"Expected a substantial number of mesh objects from a richly-authored "
            f"release-sample scene, got only {report['mesh_objects']} "
            f"(out of {report['total_objects']} total objects) -- content may have "
            f"been silently dropped"
        )

        print(f"STAB-0642: release-sample scene (medieval_castle.mc3.xml) imports cleanly "
              f"into Blender ({report['mesh_objects']} mesh objects, "
              f"{report['total_objects']} total objects, import result FINISHED) — PASS")
    finally:
        for p in (glb_path, helper_path, report_path):
            if os.path.exists(p):
                os.remove(p)


if __name__ == "__main__":
    main()
