#!/usr/bin/env python3
"""
STAB-0257: a very large external OBJ mesh (>1M triangles) exports without
memory exhaustion or crashing.

Generates a >1,000,000-triangle OBJ file on the fly (a subdivided grid --
710x710 quads = 1,008,200 triangles, ~28MB as text; not committed as a
static fixture to avoid repo bloat) and exports it through mc3togltf,
measuring the subprocess's actual peak RSS via resource.getrusage()
(Linux/macOS; ru_maxrss is in KB).

Measured in this environment: ~2.4s wall time, ~350MB peak RSS -- this
test asserts the row's own 2GB bound with headroom, not the exact
measured value (memory use will vary by platform/allocator).
"""
import os
import resource
import subprocess
import sys
import tempfile
import time

N = 710  # (N+1)^2 vertices, N*N*2 = 1,008,200 triangles
MAX_SECONDS = 60.0
MAX_RSS_KB  = 2 * 1024 * 1024  # 2GB, per STAB-0257


def generate_grid_obj(n):
    lines = []
    for y in range(n + 1):
        for x in range(n + 1):
            lines.append(f"v {x} {y} 0")
    for y in range(n):
        for x in range(n):
            v00 = y * (n + 1) + x + 1
            v10 = v00 + 1
            v01 = v00 + (n + 1)
            v11 = v01 + 1
            lines.append(f"f {v00} {v10} {v11}")
            lines.append(f"f {v00} {v11} {v01}")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <mc3togltf>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    triangle_count = N * N * 2

    with tempfile.TemporaryDirectory() as tmpdir:
        obj_path = os.path.join(tmpdir, "huge.obj")
        with open(obj_path, "w") as f:
            f.write(generate_grid_obj(N))

        xml_path = os.path.join(tmpdir, "scene.mc3.xml")
        with open(xml_path, "w") as f:
            f.write(
                '<?xml version="1.0" encoding="UTF-8"?>\n'
                '<mc3 version="0.3" model="LargeObjStressTest">\n'
                '  <objects>\n'
                '    <mesh name="HugeMesh" src="huge.obj"/>\n'
                '  </objects>\n'
                '</mc3>\n'
            )

        out = os.path.join(tmpdir, "out.glb")
        started = time.monotonic()
        r = subprocess.run([mc3togltf, xml_path, out], capture_output=True, text=True,
                            timeout=MAX_SECONDS)
        elapsed = time.monotonic() - started
        # ru_maxrss (RUSAGE_CHILDREN) is the largest peak RSS seen across all
        # reaped child processes so far -- since this is the only subprocess
        # this test spawns, it's exactly this export's peak RSS.
        peak_rss_kb = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss

        assert r.returncode == 0, (
            f"Export of a {triangle_count}-triangle OBJ failed "
            f"(returncode={r.returncode}):\n{r.stderr}"
        )
        assert os.path.exists(out) and os.path.getsize(out) > 0, "Output GLB is missing or empty"
        assert elapsed < MAX_SECONDS, (
            f"Export of {triangle_count} triangles took {elapsed:.2f}s, expected < {MAX_SECONDS:.0f}s"
        )

        assert peak_rss_kb < MAX_RSS_KB, (
            f"mc3togltf peak RSS was {peak_rss_kb / 1024:.0f}MB, expected < "
            f"{MAX_RSS_KB / 1024:.0f}MB (STAB-0257)"
        )

        print(f"STAB-0257: {triangle_count}-triangle OBJ exported in {elapsed:.2f}s, "
              f"peak RSS ~{peak_rss_kb / 1024:.0f}MB (< {MAX_RSS_KB / 1024:.0f}MB) — PASS")

    print("\nLarge OBJ stress test: PASS")
