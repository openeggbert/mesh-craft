#!/usr/bin/env python3
"""STAB-0546 -- mc3togltf CLI leaves no partial output file behind on error.

GltfExporter::exportDocument() (GltfExporter.cpp:1066-1176) builds the
entire tinygltf::Model in memory and calls WriteGltfSceneToFile() exactly
once, as the very last step -- so any failure while loading the input
document or building the model happens strictly before any file write is
attempted. This test confirms that holds for two real failure modes:

  1. Malformed input XML (Mc3Document::loadFromFile() throws before
     exportDocument() is even called).
  2. A write-protected output directory (WriteGltfSceneToFile() fails at
     the file-open step; verified empirically to create zero bytes on
     disk, not a truncated/partial file).

Usage: no_partial_output_test.py <mc3togltf> <fixture.mc3.xml>
"""
import os
import stat
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

    # STAB-0546a: malformed input XML.
    with tempfile.TemporaryDirectory() as tmpdir:
        bad_xml = os.path.join(tmpdir, "bad.mc3.xml")
        with open(bad_xml, "w") as f:
            f.write("not valid xml <<<")
        out_glb = os.path.join(tmpdir, "out.glb")

        r = run([mc3togltf, bad_xml, out_glb])
        assert r.returncode != 0, (
            f"expected a non-zero exit for malformed input XML, got 0.\n"
            f"stdout={r.stdout}\nstderr={r.stderr}"
        )
        assert r.stderr.strip(), "expected an error message on stderr for malformed input XML"
        assert not os.path.exists(out_glb), (
            "expected no output file for malformed input XML, but one was created"
        )
        print(f"PASS (STAB-0546a): malformed input rejected -- exit {r.returncode}, no output file")

    # STAB-0546b: write-protected output directory.
    with tempfile.TemporaryDirectory() as tmpdir:
        readonly_dir = os.path.join(tmpdir, "readonly")
        os.mkdir(readonly_dir)
        os.chmod(readonly_dir, stat.S_IRUSR | stat.S_IXUSR)  # r-x, no write
        try:
            out_glb = os.path.join(readonly_dir, "out.glb")
            r = run([mc3togltf, fixture, out_glb])
            assert r.returncode != 0, (
                f"expected a non-zero exit for a write-protected output path, got 0.\n"
                f"stdout={r.stdout}\nstderr={r.stderr}"
            )
            assert r.stderr.strip(), "expected an error message on stderr for a write-protected output path"
            assert not os.path.exists(out_glb), (
                "expected no output file for a write-protected output path, but one was created "
                "(even a zero-length/partial file)"
            )
            print(f"PASS (STAB-0546b): write-protected output rejected -- exit {r.returncode}, no output file")
        finally:
            os.chmod(readonly_dir, stat.S_IRWXU)  # restore so TemporaryDirectory cleanup can remove it
