#!/usr/bin/env python3
"""Real lifecycle verification for AUD-058: s_bloom.cleanup() exists but had
zero call sites, so bloom/SSAO/skybox/material-preview GL resources leaked on
every shutdown despite the destructor otherwise being deterministic.

BloomGL::leakCheck() (MeshCraftApplication.cpp) reads back every GL handle
cleanup() is supposed to have zeroed and prints "[Bloom] LEAK ..."/
"[ShadowDebug] LEAK ..." naming exactly which handle(s) survived, if any did.
This test runs the real app end-to-end (load a scene, take a screenshot,
exit -- driving the actual destructor, not a mock) and asserts neither line
ever appears in stderr. Passing the smoke test alone is NOT proof of this --
it never inspects shutdown output.

Usage: gl_shutdown_leak_test.py <MeshCraft-binary> <scene.mc3.xml>
"""
import os
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <MeshCraft-binary> <scene.mc3.xml>", file=sys.stderr)
        return 2
    binary, scene = sys.argv[1], sys.argv[2]

    with tempfile.NamedTemporaryFile(suffix=".ppm", delete=False) as f:
        shot = f.name
    try:
        cmd = [binary, scene, "--screenshot", shot]
        if subprocess_has_xvfb():
            cmd = ["xvfb-run", "--auto-servernum"] + cmd
        # Force bloom+SSAO on for this run so their GL resources actually get
        # allocated (they are UI-menu-only toggles with no CLI/scene-file
        # equivalent) -- otherwise this test would pass vacuously, never
        # having anything real to check was released.
        env = dict(os.environ)
        env["MESHCRAFT_TEST_FORCE_POSTFX"] = "1"
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=30, env=env)

        ok = True
        if r.returncode != 0:
            print(f"FAIL: app exited {r.returncode} (stderr tail: {r.stderr[-500:]!r})")
            ok = False

        leak_lines = [ln for ln in r.stderr.splitlines() if "LEAK" in ln]
        if leak_lines:
            print("FAIL: GL resource leak(s) detected at shutdown:")
            for ln in leak_lines:
                print("  " + ln)
            ok = False
        else:
            print("PASS: no '[Bloom] LEAK' / '[ShadowDebug] LEAK' lines in shutdown output")

        if ok:
            print("PASS: gl_shutdown_leak_test")
            return 0
        return 1
    finally:
        if os.path.exists(shot):
            os.unlink(shot)


def subprocess_has_xvfb():
    try:
        subprocess.run(["xvfb-run", "--help"], capture_output=True, timeout=5)
        return True
    except Exception:
        return False


if __name__ == "__main__":
    sys.exit(main())
