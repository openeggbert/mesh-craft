#!/usr/bin/env python3
"""
Regression/verification test for STAB-0526/0527/0528/0529 (editor
File -> Export GLB/glTF, invalid-extension error, exporter stats
message).

The editor's File -> Export dialog (exportGltf()/runGltfExport() in
MeshCraftApplication_FileOps.cpp) is menu/dialog-driven and wasn't
reachable from the headless --screenshot path. Added a genuine,
permanent `--export <path>` CLI flag (main.cpp, MeshCraftApplication's
3-arg constructor) that runs the same runGltfExport() codepath a menu
click would, then exits — useful for scripted/batch export, and it
makes these four rows testable:

  STAB-0526: --export out.glb  -> file created, starts with the "glTF"
             GLB magic bytes.
  STAB-0527: --export out.gltf -> file created, valid JSON.
  STAB-0528: --export out.foo  -> process exits non-zero, no file
             created, stderr has an error message (outputFormatFromPath
             rejects unknown extensions).
  STAB-0529: after a successful export, stdout contains the same
             "Exported <file> (N meshes...)" stats message shown in the
             editor's status bar, plus object/triangle/warning counts.
  AUD-037:   the fixture scene (house.mc3.xml) has an unnamed <ambient>
             light, which the exporter always warns about (STAB-0696/
             AUD-026) -- so its export status message must surface that
             warning count instead of reading as an unqualified clean
             success, matching what the editor's on-screen status bar
             now shows (runGltfExport() passes isError=true to
             setStatusMsg() when stats.warnings > 0).

Usage: editor_export_test.py <MeshCraft-binary> <scene.mc3.xml>
"""
import json
import os
import re
import subprocess
import sys
import tempfile


def run(binary, scene, export_path):
    cmd = [binary, scene, "--export", export_path]
    if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
        cmd = ["xvfb-run", "--auto-servernum"] + cmd
    return subprocess.run(cmd, capture_output=True, text=True)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <binary> <scene>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    tmpdir = tempfile.mkdtemp(prefix="meshcraft_export_")
    try:
        # STAB-0526: GLB export — file created, correct magic bytes.
        glb_path = os.path.join(tmpdir, "out.glb")
        r = run(binary, scene, glb_path)
        assert r.returncode == 0, f"GLB export failed:\nstdout={r.stdout}\nstderr={r.stderr}"
        assert os.path.exists(glb_path), "GLB export did not create the output file"
        with open(glb_path, "rb") as f:
            magic = f.read(4)
        assert magic == b"glTF", f"expected GLB magic bytes b'glTF', got {magic!r}"
        print(f"PASS (STAB-0526): {glb_path} created, magic bytes OK")

        # STAB-0529: stats message present in stdout for the same run.
        m = re.search(r"\[MeshCraft\] Exported .+\(\d+ meshes.*\).* — \d+ objects, \d+ triangles, \d+ warnings", r.stdout)
        assert m, f"expected an 'Exported ... meshes ... objects, triangles, warnings' stats line in stdout.\nstdout={r.stdout}"
        print(f"PASS (STAB-0529): stats message found — {m.group(0)}")

        # AUD-037: house.mc3.xml's unnamed ambient light always produces
        # exactly 1 export warning (AUD-026); the status message text
        # itself (not just the trailing "N warnings" tally that was always
        # printed) must call that out, since this same string is what
        # setStatusMsg() shows on-screen with the isError=true red styling.
        assert re.search(r"\[MeshCraft\] Exported .+— 1 warning \(export may be incomplete or approximate", r.stdout), (
            f"expected the status message itself (not just the trailing tally) to surface "
            f"the 1 known warning from house.mc3.xml's unnamed ambient light.\nstdout={r.stdout}"
        )
        print("PASS (AUD-037): export status message surfaces the warning count, not just the trailing tally")

        # STAB-0527: glTF (JSON) export — file created, valid JSON.
        gltf_path = os.path.join(tmpdir, "out.gltf")
        r = run(binary, scene, gltf_path)
        assert r.returncode == 0, f"glTF export failed:\nstdout={r.stdout}\nstderr={r.stderr}"
        assert os.path.exists(gltf_path), "glTF export did not create the output file"
        with open(gltf_path) as f:
            data = json.load(f)  # raises if not valid JSON
        assert "asset" in data and "meshes" in data, "exported glTF JSON missing expected top-level keys"
        print(f"PASS (STAB-0527): {gltf_path} created, valid glTF JSON")

        # STAB-0528: invalid extension — non-zero exit, no file created, error shown.
        bad_path = os.path.join(tmpdir, "out.foo")
        r = run(binary, scene, bad_path)
        assert r.returncode != 0, "expected a non-zero exit code for an unsupported export extension"
        assert not os.path.exists(bad_path), "expected no output file for an unsupported export extension"
        assert "error" in r.stderr.lower() or "unknown" in r.stderr.lower(), (
            f"expected an error message on stderr for the invalid extension, got:\nstderr={r.stderr}"
        )
        print(f"PASS (STAB-0528): invalid extension rejected — exit {r.returncode}, no file created")

    finally:
        for name in ("out.glb", "out.gltf", "out.foo", "out.bin"):
            p = os.path.join(tmpdir, name)
            if os.path.exists(p):
                os.remove(p)
        os.rmdir(tmpdir)


if __name__ == "__main__":
    main()
