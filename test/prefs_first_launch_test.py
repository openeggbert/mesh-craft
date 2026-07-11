#!/usr/bin/env python3
"""
Regression/verification test for STAB-0327 (prefs.ini auto-created on first
launch).

Before this fix, MeshCraftApplication::savePrefs() had exactly one call
site: the Preferences dialog's own "Close" button. A fresh install (no
prefs.ini on disk yet) never got one written until the user explicitly
opened and closed that dialog -- LoadContent() only ever *read* prefs via
loadPrefs(), which silently no-ops when the file doesn't exist.

LoadContent() now writes prefs.ini immediately on startup if it doesn't
already exist, right after loadPrefs(). This test runs a real MeshCraft
process against a fresh, empty XDG_CONFIG_HOME (so meshcraftConfigDir()
resolves under it, matching MeshCraftPrivate.hpp's Linux branch) and
confirms prefs.ini exists afterward, with no Preferences dialog interaction.

Usage: prefs_first_launch_test.py <MeshCraft-binary> <scene.mc3.xml>
"""
import os
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <binary> <scene>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    with tempfile.TemporaryDirectory(prefix="meshcraft_prefs_first_launch_") as config_home:
        fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_prefsfl_")
        os.close(fd)
        try:
            env = dict(os.environ)
            env["XDG_CONFIG_HOME"] = config_home

            cmd = [binary, scene, "--screenshot", ppm_path]
            if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
                cmd = ["xvfb-run", "--auto-servernum"] + cmd
            r = subprocess.run(cmd, capture_output=True, text=True, env=env)
            assert r.returncode == 0, (
                f"MeshCraft exited {r.returncode}:\nstdout={r.stdout}\nstderr={r.stderr}"
            )

            prefs_path = os.path.join(config_home, "meshcraft", "prefs.ini")
            assert os.path.exists(prefs_path), (
                f"expected {prefs_path} to exist after startup (STAB-0327: "
                f"prefs.ini should be auto-created on first launch, not only "
                f"after explicitly closing the Preferences dialog), but it "
                f"does not.\nstdout={r.stdout}\nstderr={r.stderr}"
            )
            assert os.path.getsize(prefs_path) > 0, "prefs.ini was created but is empty"

            print(f"PASS: prefs_first_launch_test — {prefs_path} auto-created on first launch")
        finally:
            if os.path.exists(ppm_path):
                os.remove(ppm_path)


if __name__ == "__main__":
    main()
