#!/usr/bin/env python3
"""
Validates mc3togltf glTF output: basic conversion and animation export.
Usage: gltf_test.py <mc3togltf-binary> <animation_test.mc3.xml> <house.mc3.xml>
"""
import json
import os
import struct
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
    check(len(anims) == 4,
          f"anim: 4 actions exported (Flash/visible skipped), got {len(anims)}")

    anim_names = {a["name"] for a in anims}
    check("Bounce"     in anim_names, "anim: Bounce exported")
    check("Spin"       in anim_names, "anim: Spin exported")
    check("Pulse"      in anim_names, "anim: Pulse exported")
    check("PivotSlide" in anim_names, "anim: PivotSlide exported")
    check("Flash"  not in anim_names, "anim: Flash (visible-only) not exported")

    # STAB-0681: autoplay/loop have no glTF core-spec equivalent -- preserved
    # via extras. All 5 actions in this fixture use loop="true", none set
    # autoplay explicitly (so it defaults false).
    for anim in anims:
        extras = anim.get("extras", {})
        check(extras.get("loop") is True,
              f"anim: {anim['name']} extras.loop==true (STAB-0681, was silently dropped)")
        check(extras.get("autoplay") is False,
              f"anim: {anim['name']} extras.autoplay==false (default, present not dropped)")

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
    # AUD-028: Spin's raw keyframes are a single 0deg->360deg sweep over just
    # 2 keyframes -- glTF LINEAR rotation interpolation is slerp, which
    # always takes the shortest arc between consecutive samples. Exported
    # as only 2 raw samples, quat(360deg) == quat(0deg) (or its antipode),
    # so a spec-compliant viewer would show NO rotation at all instead of
    # the authored full turn. The fix densely samples large-angle plain-
    # LINEAR rotation segments (same mechanism already used for cubic
    # bezier) so consecutive samples' quaternion delta stays small enough
    # for slerp to track the authored sweep -- so this now expects densely
    # sampled output, not the raw 2-keyframe count that locked in the bug.
    for ch in spin["channels"]:
        if ch["target"]["path"] == "rotation":
            ta = g["accessors"][spin["samplers"][ch["sampler"]]["input"]]
            check(ta["count"] > 2,
                  f"anim: Spin rotation (360-degree sweep) is densely sampled "
                  f"to avoid slerp shortest-path re-pathing, got {ta['count']} samples")
            # STAB-0460: Spin has time_scale="2.0" in the fixture -- exported
            # keyframe times (authored 0.0/3.0) must be baked (divided) by
            # that multiplier, so a plain glTF viewer with no notion of
            # "time scale" still reproduces the editor's real-time playback
            # speed (2x speed -> half the exported duration).
            check(ta["min"] == [0.0],
                  f"anim: Spin time accessor min==0.0 (unaffected by time_scale), got {ta['min']}")
            check(ta["max"] == [1.5],
                  f"anim: Spin time accessor max==1.5 (authored 3.0 / time_scale 2.0), got {ta['max']}")

    check(spin.get("extras", {}).get("time_scale") == 2.0,
          f"anim: Spin extras.time_scale==2.0, got {spin.get('extras', {}).get('time_scale')}")
    for anim in anims:
        if anim["name"] != "Spin":
            check(anim.get("extras", {}).get("time_scale") == 1.0,
                  f"anim: {anim['name']} extras.time_scale==1.0 (default), "
                  f"got {anim.get('extras', {}).get('time_scale')}")

    # Pulse: scale.x/y/z → merged into a single scale channel (VEC3)
    pulse = next(a for a in anims if a["name"] == "Pulse")
    paths = {ch["target"]["path"] for ch in pulse["channels"]}
    check("scale" in paths, "anim: Pulse has scale channel")
    scale_ch = [ch for ch in pulse["channels"] if ch["target"]["path"] == "scale"]
    check(len(scale_ch) == 1,
          f"anim: Pulse scale.x/y/z merged into 1 channel, got {len(scale_ch)}")

# ---------------------------------------------------------------------------
# AUD-027: pivot offset must survive into animated translation values
# ---------------------------------------------------------------------------

def read_vec3_accessor(gltf, bin_data, acc_idx):
    acc = gltf["accessors"][acc_idx]
    bv = gltf["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = bv.get("byteStride", 12)
    out = []
    for i in range(acc["count"]):
        base = offset + i * stride
        out.append(struct.unpack_from("<fff", bin_data, base))
    return out

def test_pivot_animation(binary, anim_xml):
    # GLB (not plain .gltf) so the buffer is a single inline BIN chunk --
    # simplest way to read real sample values rather than just accessor
    # metadata (count/min/max), which the checks above already cover.
    with tempfile.NamedTemporaryFile(suffix=".glb", delete=False) as f:
        out = f.name
    try:
        r = subprocess.run([binary, anim_xml, out], capture_output=True, text=True)
        check(r.returncode == 0, f"pivot-anim: mc3togltf exits 0 (stderr: {r.stderr.strip()!r})")
        if r.returncode != 0:
            return

        with open(out, "rb") as f:
            data = f.read()
        off = 12
        json_len, _ = struct.unpack_from("<II", data, off)
        gltf = json.loads(data[off + 8: off + 8 + json_len])
        off += 8 + json_len
        bin_len, _ = struct.unpack_from("<II", data, off)
        bin_data = data[off + 8: off + 8 + bin_len]

        node_by_name = {n["name"]: n for n in gltf["nodes"] if "name" in n}
        check("PivotedBox" in node_by_name, "pivot-anim: PivotedBox node present")
        if "PivotedBox" not in node_by_name:
            return
        node = node_by_name["PivotedBox"]

        # Static pose: position(5,0,0) + pivot(0.5,0,0) = 5.5,0,0 (buildNode).
        static_t = node.get("translation", [0.0, 0.0, 0.0])
        check(abs(static_t[0] - 5.5) < 1e-4,
              f"pivot-anim: PivotedBox static translation.x==5.5 (position+pivot), got {static_t[0]}")

        anims = gltf.get("animations", [])
        pivot_anim = next((a for a in anims if a["name"] == "PivotSlide"), None)
        check(pivot_anim is not None, "pivot-anim: PivotSlide animation present")
        if pivot_anim is None:
            return

        trans_ch = next((ch for ch in pivot_anim["channels"]
                          if ch["target"]["path"] == "translation"), None)
        check(trans_ch is not None, "pivot-anim: PivotSlide has a translation channel")
        if trans_ch is None:
            return

        sampler = pivot_anim["samplers"][trans_ch["sampler"]]
        values = read_vec3_accessor(gltf, bin_data, sampler["output"])
        check(len(values) >= 2, f"pivot-anim: PivotSlide translation has >=2 samples, got {len(values)}")

        # AUD-027: the authored keyframe at t=0 is position.x=5.0 (pivot-free).
        # The exported sampler's FIRST value must equal the static pose's
        # 5.5 (position+pivot) -- not the raw authored 5.0 -- so playing the
        # animation doesn't snap the object by -pivot the instant it starts.
        check(abs(values[0][0] - 5.5) < 1e-4,
              f"pivot-anim: PivotSlide sampler t=0 value.x==5.5 (matches static "
              f"pose, pivot included), got {values[0][0]} "
              f"({'BUG: raw pivot-free authored value leaked through' if abs(values[0][0] - 5.0) < 1e-4 else 'unexpected value'})")
        check(abs(values[0][0] - static_t[0]) < 1e-4,
              "pivot-anim: sampler t=0 value matches the static node translation exactly "
              f"(animated={values[0][0]}, static={static_t[0]})")
    finally:
        if os.path.exists(out):
            os.unlink(out)

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
    test_pivot_animation(binary, anim_xml)
    test_glb(binary, house_xml)

    print(f"\n{'All tests passed.' if failures == 0 else f'FAILURES: {failures}'}")
    sys.exit(1 if failures > 0 else 0)

if __name__ == "__main__":
    main()
