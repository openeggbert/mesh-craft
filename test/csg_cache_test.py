#!/usr/bin/env python3
"""
Regression/verification test for STAB-0522 (CSG preview cache: re-render
without touching CSG returns a cache hit, not a re-evaluation).

SceneRenderer's content-hash CSG cache (K1, SceneRenderer.cpp's
Union/Intersection/Difference case) recomputes buildManifoldTree() only
when the hash of the CSG subtree + its parent world matrix actually
changes. --screenshot mode runs a 120-frame warm-up (autoScreenshotCountdown_)
before capturing, so a static, never-modified CSG object gets drawn ~121
times total. csg_cache.mc3.xml holds one such static CSG union, and
MeshCraftApplication::EndDraw() prints
"[CsgCache] evaluations: N" (SceneRenderer::csgCacheEvaluationCount(),
incremented only on a cache miss) right before exiting. If the cache
works, N should be 1 (or a small constant); if it were broken and
re-evaluated every frame, N would climb toward ~120.

Usage: csg_cache_test.py <MeshCraft-binary> <csg_cache.mc3.xml>
"""
import re
import subprocess
import sys
import tempfile
import os


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <binary> <scene>")
        sys.exit(1)
    binary, scene = sys.argv[1], sys.argv[2]

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_csgcache_")
    os.close(fd)
    try:
        cmd = [binary, scene, "--screenshot", ppm_path]
        if subprocess.run(["which", "xvfb-run"], capture_output=True).returncode == 0:
            cmd = ["xvfb-run", "--auto-servernum"] + cmd
        r = subprocess.run(cmd, capture_output=True, text=True)
        assert r.returncode == 0, (
            f"MeshCraft exited {r.returncode}:\nstdout={r.stdout}\nstderr={r.stderr}"
        )
        assert os.path.getsize(ppm_path) > 0, "screenshot PPM is empty"

        m = re.search(r"\[CsgCache\] evaluations:\s*(\d+)", r.stdout)
        assert m, (
            f"expected a '[CsgCache] evaluations: N' line in stdout, found none.\n"
            f"stdout={r.stdout}"
        )
        evaluations = int(m.group(1))

        assert evaluations <= 3, (
            f"expected the static CSG object to be evaluated only once (a "
            f"small constant, not once per warm-up frame) across the "
            f"~120-frame --screenshot warm-up loop, but buildManifoldTree() "
            f"ran {evaluations} times — the content-hash cache may not be "
            f"working"
        )
        assert evaluations >= 1, (
            "expected at least one CSG evaluation (the object must be "
            "rendered at least once), found 0 — the CSG object may not be "
            "rendering at all"
        )

        print(f"PASS: csg_cache_test — {evaluations} CSG evaluation(s) across the warm-up loop")
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


if __name__ == "__main__":
    main()
