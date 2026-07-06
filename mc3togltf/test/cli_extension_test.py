#!/usr/bin/env python3
"""
STAB-0181/0182/0183/0184/0185: mc3togltf CLI output-extension handling.

  - STAB-0181: an unrecognized output extension (".foo") is rejected with a
    non-zero exit code and a named error, before any file is written.
  - STAB-0182: ".glb" produces a binary GLB (magic bytes b"glTF").
  - STAB-0183: ".gltf" produces JSON glTF (parses, has "asset"/"version").
  - STAB-0184: uppercase ".GLB" is accepted and treated identically to
    lowercase ".glb" (outputFormatFromPath() lowercases the extension
    before comparing).
  - STAB-0185: re-exporting to a path that already exists silently
    overwrites it (no prompt, no error) — confirmed by exporting two
    different source scenes to the same output path and checking the
    file's content actually changed to match the second export.
"""
import json
import os
import struct
import subprocess
import sys
import tempfile

GLTF_MAGIC = 0x46546C67  # "glTF" little-endian uint32


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <house.mc3.xml> <all_primitives.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    house_xml = sys.argv[2]
    prims_xml = sys.argv[3]

    with tempfile.TemporaryDirectory() as tmpdir:
        # STAB-0181: unrecognized extension rejected, non-zero exit, named error.
        out_foo = os.path.join(tmpdir, "out.foo")
        r = run([mc3togltf, house_xml, out_foo])
        assert r.returncode != 0, f"Expected non-zero exit for '.foo' output, got 0"
        combined = r.stdout + r.stderr
        assert "Unknown output extension" in combined and ".foo" in combined, (
            f"Expected a named 'Unknown output extension ... .foo' error, got:\n{combined}"
        )
        assert not os.path.exists(out_foo), "No output file should be written for a rejected extension"
        print("STAB-0181: '.foo' extension rejected with named error, no file written — PASS")

        # STAB-0182: ".glb" -> binary GLB.
        out_glb = os.path.join(tmpdir, "out.glb")
        r = run([mc3togltf, house_xml, out_glb])
        assert r.returncode == 0, f"Expected '.glb' export to succeed:\n{r.stderr}"
        with open(out_glb, "rb") as f:
            magic, version, _length = struct.unpack_from("<III", f.read(12), 0)
        assert magic == GLTF_MAGIC, f"Expected GLB magic {GLTF_MAGIC:#x}, got {magic:#x}"
        print("STAB-0182: '.glb' produces a binary GLB with correct magic — PASS")

        # STAB-0183: ".gltf" -> JSON glTF with required top-level keys.
        out_gltf = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, house_xml, out_gltf])
        assert r.returncode == 0, f"Expected '.gltf' export to succeed:\n{r.stderr}"
        with open(out_gltf) as f:
            gltf = json.load(f)
        assert gltf.get("asset", {}).get("version") == "2.0", (
            f"Expected asset.version == '2.0', got: {gltf.get('asset')}"
        )
        print("STAB-0183: '.gltf' produces valid JSON glTF — PASS")

        # STAB-0184: uppercase ".GLB" treated the same as lowercase ".glb".
        out_glb_upper = os.path.join(tmpdir, "out_upper.GLB")
        r = run([mc3togltf, house_xml, out_glb_upper])
        assert r.returncode == 0, f"Expected uppercase '.GLB' export to succeed:\n{r.stderr}"
        assert os.path.exists(out_glb_upper), "Uppercase '.GLB' output file was not written"
        with open(out_glb_upper, "rb") as f:
            magic, _version, _length = struct.unpack_from("<III", f.read(12), 0)
        assert magic == GLTF_MAGIC, (
            f"Expected uppercase '.GLB' to still produce binary GLB (magic {GLTF_MAGIC:#x}), got {magic:#x}"
        )
        print("STAB-0184: uppercase '.GLB' accepted, produces binary GLB — PASS")

        # STAB-0185: re-exporting a different scene to the same path silently overwrites.
        shared_path = os.path.join(tmpdir, "shared.gltf")
        r1 = run([mc3togltf, house_xml, shared_path])
        assert r1.returncode == 0, f"First export to shared path failed:\n{r1.stderr}"
        with open(shared_path) as f:
            first_content = f.read()

        r2 = run([mc3togltf, prims_xml, shared_path])
        assert r2.returncode == 0, (
            f"Expected re-export to an existing path to succeed silently (no "
            f"prompt/error), got returncode={r2.returncode}:\n{r2.stderr}"
        )
        combined2 = r2.stdout + r2.stderr
        assert "overwrite" not in combined2.lower() and "exists" not in combined2.lower(), (
            f"Expected a silent overwrite with no 'overwrite'/'exists' prompt text, got:\n{combined2}"
        )
        with open(shared_path) as f:
            second_content = f.read()
        assert second_content != first_content, (
            "Expected the second export's different source scene to actually "
            "replace the file's content (real overwrite, not a no-op or append)"
        )
        print("STAB-0185: re-export to an existing path silently overwrites it — PASS")

    print("\nCLI output-extension handling test: PASS")
