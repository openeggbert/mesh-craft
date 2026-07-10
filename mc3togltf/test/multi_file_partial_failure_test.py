#!/usr/bin/env python3
"""STAB-0689 -- characterize the non-atomic plain-.gltf multi-file write path.

GltfExporter.cpp:1367-1376 (the `else` branch of exportDocument(), taken for
plain .gltf output, as opposed to .glb) writes the JSON and the external
.bin buffer as two separate files, unlike the .glb path which writes to a
temp file and renames atomically (STAB-0546/AUDIT-0019). This was never
tested for what actually happens on a *mid*-write failure (one file
succeeds, the other doesn't) -- this test characterizes that behavior
directly, empirically, rather than assuming either "it's fine" or "it's
broken".

Reading tinygltf's own WriteGltfSceneToFile() (tiny_gltf.h) shows it writes
buffers/images to disk FIRST, then the JSON file LAST. That predicts two
distinct outcomes depending on which write is blocked, both confirmed here:

  1. If the .bin write fails, WriteGltfSceneToFile() returns false before
     ever attempting the JSON write -- no .gltf file is left on disk at all.
     Clean, safe failure.

  2. If the .gltf JSON write fails (after the .bin write already
     succeeded), a complete, valid .bin file IS left on disk with no
     corresponding .gltf referencing it -- this IS the non-atomic gap the
     code comment warns about. It's a relatively benign failure mode (an
     orphaned, unreferenced file -- not a .gltf that looks complete but
     points at a missing/truncated buffer), and it self-heals on a
     successful retry to the same path, but it does leave stray output.

Usage: multi_file_partial_failure_test.py <mc3togltf> <fixture.mc3.xml>
"""
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <fixture.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    fixture = sys.argv[2]

    # STAB-0689a: block the .bin write (pre-create its path as a directory,
    # so tinygltf's file-open for writing fails there) -- the .gltf JSON's
    # own path is left free.
    with tempfile.TemporaryDirectory() as tmpdir:
        out_gltf = os.path.join(tmpdir, "out.gltf")
        os.mkdir(os.path.join(tmpdir, "out.bin"))

        r = run([mc3togltf, fixture, out_gltf])
        assert r.returncode != 0, (
            f"expected non-zero exit when the .bin sibling path is blocked, got 0.\n"
            f"stdout={r.stdout}\nstderr={r.stderr}"
        )
        assert r.stderr.strip(), "expected an error message on stderr"
        assert not os.path.exists(out_gltf), (
            "STAB-0689a: expected no .gltf JSON file when the .bin write fails "
            "(tinygltf writes buffers before JSON, so it should abort before "
            "ever touching the JSON path) -- but one was created"
        )
        print("PASS (STAB-0689a): .bin write blocked -> clean failure, "
              "no .gltf JSON left behind")

    # STAB-0689b: block the .gltf JSON write (pre-create its path as a
    # directory) -- the .bin sibling path is left free, so it succeeds
    # before the JSON write is attempted and fails.
    with tempfile.TemporaryDirectory() as tmpdir:
        out_gltf = os.path.join(tmpdir, "out.gltf")
        out_bin = os.path.join(tmpdir, "out.bin")
        os.mkdir(out_gltf)

        r = run([mc3togltf, fixture, out_gltf])
        assert r.returncode != 0, (
            f"expected non-zero exit when the .gltf JSON path is blocked, got 0.\n"
            f"stdout={r.stdout}\nstderr={r.stderr}"
        )
        assert r.stderr.strip(), "expected an error message on stderr"
        # This assertion documents/locks in the CURRENT (imperfect, known)
        # non-atomic behavior -- a valid .bin IS left on disk even though
        # the overall export reported failure. If this ever changes (e.g. a
        # future fix makes the .bin write itself transactional/cleaned-up on
        # JSON failure), update this assertion to match the improved
        # behavior rather than treating a change here as a regression.
        assert os.path.exists(out_bin) and os.path.getsize(out_bin) > 0, (
            "STAB-0689b: expected the known non-atomic gap -- a complete "
            ".bin file left on disk with no corresponding .gltf JSON -- but "
            "no .bin was found. If this now passes because the .bin was "
            "cleaned up, that's an improvement: update this test."
        )
        print("PASS (STAB-0689b): .gltf JSON write blocked -> failure reported, "
              "but a valid orphaned .bin is left behind (documented non-atomic "
              "gap, not a worse corruption)")

    print("\nMulti-file partial-failure characterization: PASS")
