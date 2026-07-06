#!/usr/bin/env python3
"""
Validates mc3togltf glTF output: basic conversion and animation export.
Usage: gltf_test.py <mc3togltf-binary> <animation_test.mc3.xml> <house.mc3.xml>
"""
import json
import os
import subprocess
import sys
import tempfile

failures = 0

def check(cond, msg):
    global failures
    if cond:
        print(f"PASS: {msg}")
    else:
        print(f"FAIL: {msg}", file=sys.stderr)
        failures += 1

def convert(binary, src):
    """Run mc3togltf, return parsed glTF dict or None on error."""
    with tempfile.NamedTemporaryFile(suffix=".gltf", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, src, out], capture_output=True, text=True)
        check(r.returncode == 0,
              f"mc3togltf exits 0 on {os.path.basename(src)} (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            return None
        with open(out) as f:
            return json.load(f)
    finally:
        if os.path.exists(out):
            os.unlink(out)

# ---------------------------------------------------------------------------
# Basic conversion test (house.mc3.xml)
# ---------------------------------------------------------------------------

def test_basic(binary, house_xml):
    g = convert(binary, house_xml)
    if g is None:
        return
    check(g.get("asset", {}).get("version") == "2.0",   "basic: asset.version==2.0")
    check(len(g.get("nodes", [])) > 0,                  "basic: nodes present")
    check(len(g.get("meshes", [])) > 0,                 "basic: meshes present")
    check(g.get("scene") == 0,                          "basic: scene==0 (default scene)")
    check(len(g.get("scenes", [])) == 1,
          f"basic: exactly 1 scene in scenes array, got {len(g.get('scenes', []))}")
    check("mc3_model" in g.get("asset", {}).get("extras", {}),
          "basic: asset.extras.mc3_model present")

# ---------------------------------------------------------------------------
# Animation export test (animation_test.mc3.xml)
# ---------------------------------------------------------------------------

def test_animation(binary, anim_xml):
    g = convert(binary, anim_xml)
    if g is None:
        return

    anims = g.get("animations", [])
    # Flash action uses only 'visible' which is not a glTF transform → skipped
    check(len(anims) == 3,
          f"anim: 3 actions exported (Flash/visible skipped), got {len(anims)}")

    anim_names = {a["name"] for a in anims}
    check("Bounce" in anim_names,     "anim: Bounce exported")
    check("Spin"   in anim_names,     "anim: Spin exported")
    check("Pulse"  in anim_names,     "anim: Pulse exported")
    check("Flash"  not in anim_names, "anim: Flash (visible-only) not exported")

    for anim in anims:
        name = anim["name"]
        check(len(anim.get("samplers", [])) > 0, f"anim: {name} has samplers")
        check(len(anim.get("channels", [])) > 0, f"anim: {name} has channels")

        for ch in anim["channels"]:
            s    = anim["samplers"][ch["sampler"]]
            path = ch["target"]["path"]
            node = g["nodes"][ch["target"]["node"]]["name"]

            # Time (input) accessor: SCALAR with required min/max
            ta = g["accessors"][s["input"]]
            check(ta["type"] == "SCALAR",
                  f"anim: {name}/{path} time accessor is SCALAR")
            check("min" in ta and "max" in ta,
                  f"anim: {name}/{path} time accessor has min/max (glTF spec required)")
            check(ta["count"] >= 2,
                  f"anim: {name}/{path} time accessor has >=2 samples, got {ta['count']}")

            # Value (output) accessor type: rotation→VEC4, others→VEC3
            va = g["accessors"][s["output"]]
            expected = "VEC4" if path == "rotation" else "VEC3"
            check(va["type"] == expected,
                  f"anim: {name}/{path} output type=={expected}, got {va['type']}")

            check(s["interpolation"] in ("LINEAR", "STEP", "CUBICSPLINE"),
                  f"anim: {name}/{path} interpolation valid, got {s['interpolation']!r}")

    # Bounce: position.y → translation
    bounce = next(a for a in anims if a["name"] == "Bounce")
    paths = {ch["target"]["path"] for ch in bounce["channels"]}
    check("translation" in paths, "anim: Bounce has translation channel")
    # cubic bezier → dense samples, expect >2 time steps
    for ch in bounce["channels"]:
        ta = g["accessors"][bounce["samplers"][ch["sampler"]]["input"]]
        check(ta["count"] > 2, f"anim: Bounce translation densely sampled ({ta['count']} samples)")

    # Spin: rotation.y → rotation (VEC4 quaternion)
    spin = next(a for a in anims if a["name"] == "Spin")
    paths = {ch["target"]["path"] for ch in spin["channels"]}
    check("rotation" in paths, "anim: Spin has rotation channel")
    # linear 2-keyframe → exactly 2 time steps
    for ch in spin["channels"]:
        if ch["target"]["path"] == "rotation":
            ta = g["accessors"][spin["samplers"][ch["sampler"]]["input"]]
            check(ta["count"] == 2,
                  f"anim: Spin rotation has exactly 2 samples (linear), got {ta['count']}")

    # Pulse: scale.x/y/z → merged into a single scale channel (VEC3)
    pulse = next(a for a in anims if a["name"] == "Pulse")
    paths = {ch["target"]["path"] for ch in pulse["channels"]}
    check("scale" in paths, "anim: Pulse has scale channel")
    scale_ch = [ch for ch in pulse["channels"] if ch["target"]["path"] == "scale"]
    check(len(scale_ch) == 1,
          f"anim: Pulse scale.x/y/z merged into 1 channel, got {len(scale_ch)}")

# ---------------------------------------------------------------------------
# GLB (binary) output smoke test
# ---------------------------------------------------------------------------

def test_glb(binary, house_xml):
    with tempfile.NamedTemporaryFile(suffix=".glb", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, house_xml, out], capture_output=True, text=True)
        check(r.returncode == 0,  "glb: mc3togltf exits 0")
        size = os.path.getsize(out) if os.path.exists(out) else 0
        check(size > 0,           f"glb: output non-empty ({size} bytes)")
        # GLB magic: first 4 bytes = 0x676C5446 ("glTF")
        with open(out, "rb") as f:
            magic = f.read(4)
        check(magic == b"glTF", f"glb: GLB magic bytes correct ({magic!r})")
    finally:
        if os.path.exists(out):
            os.unlink(out)

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 4:
        print("Usage: gltf_test.py <binary> <animation_test.mc3.xml> <house.mc3.xml>",
              file=sys.stderr)
        sys.exit(1)

    binary   = sys.argv[1]
    anim_xml = sys.argv[2]
    house_xml = sys.argv[3]

    test_basic(binary, house_xml)
    test_animation(binary, anim_xml)
    test_glb(binary, house_xml)

    print(f"\n{'All tests passed.' if failures == 0 else f'FAILURES: {failures}'}")
    sys.exit(1 if failures > 0 else 0)

if __name__ == "__main__":
    main()
