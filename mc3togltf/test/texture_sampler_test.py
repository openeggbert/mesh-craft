#!/usr/bin/env python3
"""
STAB-0417/STAB-0418: texture wrap_u/wrap_v/filter must export to the correct
glTF sampler wrapS/wrapT/minFilter/magFilter enum values.

mc3.xsd's wrapModeType allows "repeat"/"clamp"/"mirror" and filterType allows
"linear"/"nearest" — the exporter previously only distinguished "clamp" from
everything else (mirror silently became REPEAT) and ignored the filter
attribute entirely (every sampler got hardcoded LINEAR/LINEAR_MIPMAP_LINEAR).
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

    print("\nTexture sampler (wrap/filter) export test: PASS")
