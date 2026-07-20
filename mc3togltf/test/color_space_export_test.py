#!/usr/bin/env python3
"""
SYS-W14-23 (2026-07-20): Mc3Texture::color_space ("srgb"/"linear") round-
trips and is editable, but GltfExporter never read it back -- no warning
was ever emitted when a texture's declared color_space conflicts with the
fixed, spec-mandated glTF 2.0 per-slot encoding (baseColorTexture/
emissiveTexture must be sRGB; normalTexture/metallicRoughnessTexture/
occlusionTexture must be linear -- glTF 2.0 has no per-texture color-space
override, so the exporter always follows the slot's mandated convention
regardless of what's declared).

mat_matched's textures declare the correct color_space for their slot
(base_color_texture=srgb, normal_texture=linear) -- no warning expected.
mat_mismatched's textures declare the wrong one for their slot
(base_color_texture=linear, normal_texture=srgb) -- both directions of
the mismatch (sRGB slot given linear, linear slot given sRGB) must warn.
Export must still succeed (exit 0) either way -- this is an informational
warning, not a validation failure.
"""
import os
import subprocess
import sys
import tempfile


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <color_space_export.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, f"Export failed (returncode={r.returncode}):\n{r.stderr}"
        assert os.path.exists(out) and os.path.getsize(out) > 0, "Output glTF is missing or empty"

        combined = r.stdout + r.stderr

        # mat_matched: both textures declare the correct color_space for
        # their slot -- no color-space-mismatch warning for either.
        for tex_id in ("tex_base_ok", "tex_normal_ok"):
            mismatch_lines = [
                line for line in combined.splitlines()
                if tex_id in line and "declares color_space" in line
            ]
            assert not mismatch_lines, (
                f"{tex_id} correctly declares its slot's mandated color_space -- "
                f"expected no color-space-mismatch warning, got: {mismatch_lines}"
            )
        print("mat_matched: no color-space-mismatch warnings — PASS")

        # mat_mismatched: both textures declare the WRONG color_space for
        # their slot -- both directions of the mismatch must warn.
        expected = {
            "tex_base_wrong":   ("base_color_texture", "linear", "srgb"),
            "tex_normal_wrong": ("normal_texture", "srgb", "linear"),
        }
        for tex_id, (slot, declared, required) in expected.items():
            mismatch_lines = [
                line for line in combined.splitlines()
                if tex_id in line and "declares color_space" in line
            ]
            assert mismatch_lines, (
                f"Expected a color-space-mismatch warning for '{tex_id}' (declared "
                f"{declared}, slot {slot} requires {required}), got none. Full output:\n{combined}"
            )
            line = mismatch_lines[0]
            assert slot in line, f"{tex_id} warning should name its slot '{slot}': {line}"
            assert f'color_space="{declared}"' in line, (
                f"{tex_id} warning should quote its declared color_space '{declared}': {line}"
            )
            assert required in line, (
                f"{tex_id} warning should name the required color_space '{required}': {line}"
            )
            print(f"{tex_id}: {line.strip()} — PASS")

    print("\nColor-space slot-mismatch warning (SYS-W14-23) test: PASS")
