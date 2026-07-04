#!/usr/bin/env python3
"""
STAB-0417/STAB-0418: texture wrap_u/wrap_v/filter must export to the correct
glTF sampler wrapS/wrapT/minFilter/magFilter enum values.

mc3.xsd's wrapModeType allows "repeat"/"clamp"/"mirror" and filterType allows
"linear"/"nearest" — the exporter previously only distinguished "clamp" from
everything else (mirror silently became REPEAT) and ignored the filter
attribute entirely (every sampler got hardcoded LINEAR/LINEAR_MIPMAP_LINEAR).

STAB-0420: all 4 textures in the fixture reference nonexistent files
(textures/a.png etc.) — export must still succeed (exit 0), warning in
--embed/.glb mode where the exporter tries to read pixel bytes.

STAB-0416: the exported glTF image "uri" must preserve the texture's full
relative path (e.g. "textures/a.png"), not just its basename. tinygltf's
default image writer (tinygltf::WriteImageData) truncates external-reference
URIs down to GetBaseFilename() — silently dropping any subdirectory prefix —
so GltfExporter.cpp installs a custom SetImageWriter() override for the
external-reference case.
"""
import json
import os
import subprocess
import sys
import tempfile

# glTF 2.0 sampler enum values (WebGL/OpenGL constants)
REPEAT = 10497
CLAMP_TO_EDGE = 33071
MIRRORED_REPEAT = 33648
NEAREST = 9728
LINEAR = 9729
NEAREST_MIPMAP_NEAREST = 9984
LINEAR_MIPMAP_LINEAR = 9987

EXPECTED = {
    "tex_repeat":  {"wrapS": REPEAT,          "wrapT": REPEAT,          "minFilter": LINEAR_MIPMAP_LINEAR,     "magFilter": LINEAR},
    "tex_clamp":   {"wrapS": CLAMP_TO_EDGE,   "wrapT": CLAMP_TO_EDGE,   "minFilter": LINEAR_MIPMAP_LINEAR,     "magFilter": LINEAR},
    "tex_mirror":  {"wrapS": MIRRORED_REPEAT, "wrapT": MIRRORED_REPEAT, "minFilter": LINEAR_MIPMAP_LINEAR,     "magFilter": LINEAR},
    "tex_nearest": {"wrapS": REPEAT,          "wrapT": REPEAT,          "minFilter": NEAREST_MIPMAP_NEAREST,   "magFilter": NEAREST},
}

EXPECTED_URI = {
    "tex_repeat":  "textures/a.png",
    "tex_clamp":   "textures/b.png",
    "tex_mirror":  "textures/c.png",
    "tex_nearest": "textures/d.png",
}


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <mc3togltf> <texture_sampler.mc3.xml>")
        sys.exit(1)

    mc3togltf = sys.argv[1]
    xml_path  = sys.argv[2]

    with tempfile.TemporaryDirectory() as tmpdir:
        out = os.path.join(tmpdir, "out.gltf")
        r = run([mc3togltf, xml_path, out])
        assert r.returncode == 0, f"Export failed (returncode={r.returncode}):\n{r.stderr}"
        assert os.path.exists(out) and os.path.getsize(out) > 0, "Output glTF is missing or empty"

        with open(out) as f:
            gltf = json.load(f)

        images    = gltf.get("images", [])
        textures  = gltf.get("textures", [])
        samplers  = gltf.get("samplers", [])

        # STAB-0416: full relative path must survive export, not just basename.
        img_by_name = {img.get("name", ""): img for img in images}
        for tex_id, expected_uri in EXPECTED_URI.items():
            assert tex_id in img_by_name, (
                f"Expected image '{tex_id}' in output, got: {sorted(img_by_name.keys())}"
            )
            actual_uri = img_by_name[tex_id].get("uri")
            assert actual_uri == expected_uri, (
                f"{tex_id}.uri: expected '{expected_uri}' (full relative path), "
                f"got '{actual_uri}' (STAB-0416: subdirectory prefix must survive export)"
            )
            print(f"{tex_id}: uri={actual_uri} — PASS")

        # Map texture name (mc3 id, stored as glTF image name) -> sampler dict,
        # via the glTF texture that references that image.
        img_name_to_idx = {img.get("name", ""): i for i, img in enumerate(images)}
        name_to_sampler = {}
        for name, img_idx in img_name_to_idx.items():
            tex = next((t for t in textures if t.get("source") == img_idx), None)
            assert tex is not None, f"No glTF texture references image '{name}'"
            samp_idx = tex.get("sampler")
            assert samp_idx is not None and 0 <= samp_idx < len(samplers), (
                f"Texture for '{name}' has no valid sampler index"
            )
            name_to_sampler[name] = samplers[samp_idx]

        for tex_id, expected in EXPECTED.items():
            assert tex_id in name_to_sampler, (
                f"Expected texture '{tex_id}' in output, got: {sorted(name_to_sampler.keys())}"
            )
            samp = name_to_sampler[tex_id]
            for key, expected_val in expected.items():
                actual_val = samp.get(key)
                assert actual_val == expected_val, (
                    f"{tex_id}.{key}: expected {expected_val}, got {actual_val} (sampler={samp})"
                )
            print(f"{tex_id}: wrapS={samp['wrapS']} wrapT={samp['wrapT']} "
                  f"minFilter={samp['minFilter']} magFilter={samp['magFilter']} — PASS")

        # STAB-0420: missing texture files must warn, not fail, in --embed/.glb mode.
        out_glb = os.path.join(tmpdir, "out.glb")
        r_glb = run([mc3togltf, xml_path, out_glb])
        assert r_glb.returncode == 0, (
            f"Expected export to succeed despite missing texture files, "
            f"got returncode={r_glb.returncode}\nstderr: {r_glb.stderr}"
        )
        assert os.path.exists(out_glb) and os.path.getsize(out_glb) > 0, \
            "Output GLB is missing or empty"
        combined = r_glb.stdout + r_glb.stderr
        assert combined.count("not found") >= 4, (
            f"Expected a 'not found' warning for each of the 4 missing texture "
            f"files, got:\n{combined}"
        )
        print(f"Missing-texture warning (--embed/.glb mode): PASS "
              f"({combined.count('not found')} warnings, exit 0)")

    print("\nTexture sampler (wrap/filter/uri) export + missing-texture-warns test: PASS")
