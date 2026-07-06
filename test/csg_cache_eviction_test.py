#!/usr/bin/env python3
"""
STAB-0216: SceneRenderer's CSG mesh cache (csgMeshCache_) never grows
unbounded. It is cleared entirely once it would exceed 128 entries (a
full-clear policy, not LRU eviction down to a fixed size) — see
csgMeshCacheSize() in SceneRenderer.hpp.

csg_cache_eviction.mc3.xml holds 150 CSG unions, each with distinct
geometry (so each produces a unique content hash / cache key). Rendering
this scene forces well over 128 distinct cache entries across a single
--screenshot run's warm-up loop. MeshCraftApplication::EndDraw() prints
"[CsgCache] size: N" (SceneRenderer::csgMeshCacheSize()) right before
exiting -- this test confirms N never exceeds 129 (128 existing + 1
just-inserted, checked before the *next* insertion triggers a clear).

Usage: csg_cache_eviction_test.py <MeshCraft-binary> <csg_cache_eviction.mc3.xml>
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

    fd, ppm_path = tempfile.mkstemp(suffix=".ppm", prefix="meshcraft_csgevict_")
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

        m = re.search(r"\[CsgCache\] size:\s*(\d+)", r.stdout)
        assert m, (
            f"expected a '[CsgCache] size: N' line in stdout, found none.\n"
            f"stdout={r.stdout}"
        )
        cache_size = int(m.group(1))

        assert cache_size <= 129, (
            f"expected csgMeshCache_ to never exceed 129 entries (128 + 1 "
            f"just-inserted before the next clear) even with 150 unique CSG "
            f"nodes in the scene, but printed size was {cache_size} -- the "
            f"cache may be growing unbounded"
        )

        print(f"PASS: csg_cache_eviction_test — cache size {cache_size} "
              f"(<= 129) with 150 unique CSG nodes rendered")
    finally:
        if os.path.exists(ppm_path):
            os.remove(ppm_path)


if __name__ == "__main__":
    main()
