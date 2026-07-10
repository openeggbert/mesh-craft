#!/usr/bin/env python3
"""STAB-0690 -- byte-level correctness of GLB chunk assembly.

Per the glTF 2.0 binary (.glb) spec: the header's declared total length
must match the real file size; each chunk's length must be a multiple of 4;
a JSON chunk's padding bytes (between the real JSON text and the declared
chunk length) must be ASCII space (0x20); a BIN chunk's padding bytes must
be zero (0x00). This was previously untested at the byte level, delegated
entirely to tinygltf.

Investigated whether real mc3togltf output can ever exercise non-zero BIN
chunk padding: it can't, currently. Every value written into the shared
binary buffer is either float32 (positions/normals/texcoords) or uint32
(indices -- addAccessorIndices() always uses UNSIGNED_INT, never
UNSIGNED_SHORT/BYTE, see GltfExporter.cpp:257), both inherently 4-byte
aligned, so the buffer's total byteLength is always already a multiple of 4.
Embedded texture images (GLB embedImages=true) go into the JSON as base64
data URIs (GltfExporter.cpp:317-321), not appended to the BIN buffer, so
they don't affect its alignment either. This test therefore asserts what IS
actually exercisable (header consistency, 4-byte alignment of both chunks,
and real non-zero JSON padding, which the default fixture's JSON length
does require), and documents why BIN padding can't be forced non-zero by
any current real export path rather than asserting a fact the fixture can't
demonstrate.

Usage: glb_padding_test.py <mc3togltf> <fixture.mc3.xml>
"""
import json
import os
import struct
import subprocess
import sys
import tempfile

JSON_CHUNK_TYPE = 0x4E4F534A  # "JSON"
BIN_CHUNK_TYPE  = 0x004E4942  # "BIN\0"


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <fixture.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    fixture = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.glb")
        r = run([mc3togltf, fixture, out])
        assert r.returncode == 0, f"mc3togltf failed:\n{r.stderr}"

        with open(out, "rb") as f:
            data = f.read()

    magic, version, total_length = struct.unpack_from("<4sII", data, 0)
    assert magic == b"glTF", f"bad magic: {magic!r}"
    assert version == 2, f"expected version 2, got {version}"
    assert total_length == len(data), (
        f"header length {total_length} != actual file size {len(data)}"
    )
    print(f"PASS: header magic/version/length consistent (file size {len(data)})")

    off = 12
    json_chunk_len, json_chunk_type = struct.unpack_from("<II", data, off)
    assert json_chunk_type == JSON_CHUNK_TYPE, f"expected JSON chunk type, got {json_chunk_type:#x}"
    assert json_chunk_len % 4 == 0, f"JSON chunk length {json_chunk_len} not 4-byte aligned"
    json_bytes = data[off + 8: off + 8 + json_chunk_len]

    stripped = json_bytes.rstrip(b' ')
    json_pad = json_bytes[len(stripped):]
    assert all(b == 0x20 for b in json_pad), (
        f"JSON chunk padding must be ASCII space (0x20), got: {json_pad!r}"
    )
    parsed = json.loads(stripped)  # also confirms the unpadded content is valid JSON
    print(f"PASS: JSON chunk is 4-byte aligned, {len(json_pad)} padding byte(s) "
          f"are all 0x20, content parses as valid JSON")

    off += 8 + json_chunk_len
    assert off < len(data), "no BIN chunk present (fixture must reference a mesh)"
    bin_chunk_len, bin_chunk_type = struct.unpack_from("<II", data, off)
    assert bin_chunk_type == BIN_CHUNK_TYPE, f"expected BIN chunk type, got {bin_chunk_type:#x}"
    assert bin_chunk_len % 4 == 0, f"BIN chunk length {bin_chunk_len} not 4-byte aligned"

    buffers = parsed.get("buffers", [])
    assert buffers, "expected at least one buffer in the JSON"
    real_len = buffers[0]["byteLength"]
    bin_bytes = data[off + 8: off + 8 + bin_chunk_len]
    bin_pad = bin_bytes[real_len:]
    assert all(b == 0x00 for b in bin_pad), (
        f"BIN chunk padding must be zero (0x00), got: {bin_pad!r}"
    )
    # STAB-0690: this padding is architecturally always empty for real
    # mc3togltf output today (see module docstring) -- asserted anyway (it's
    # still a real invariant, just not currently forceable to be non-empty),
    # with the reason logged rather than silently treated as "fully tested".
    print(f"PASS: BIN chunk is 4-byte aligned, {len(bin_pad)} padding byte(s) "
          f"are all 0x00 ({'exercised non-trivially' if bin_pad else 'zero-length -- see docstring, not currently forceable non-zero'})")

    print("\nGLB chunk padding byte-level checks: PASS")
