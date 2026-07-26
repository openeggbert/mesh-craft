#!/usr/bin/env python3
"""Bounded source-to-document checks for mechanically verifiable claims.

This is deliberately a small drift detector, not an attempt to prove all
documentation semantically true. It protects the capabilities that have
already regressed into stale prose: Walk Mode proxy support, Emscripten IDBFS,
autosave recovery, ordinary-object viewport UV mapping, and document rotation
conventions, and the bounded Event Preview/Play contract.
"""
from pathlib import Path
import sys


def require(text: str, needle: str, description: str) -> int:
    if needle in text:
        print(f"PASS: {description}")
        return 0
    print(f"FAIL: {description} (missing: {needle!r})", file=sys.stderr)
    return 1


def forbid(text: str, needle: str, description: str) -> int:
    if needle not in text:
        print(f"PASS: {description}")
        return 0
    print(f"FAIL: {description} (stale claim: {needle!r})", file=sys.stderr)
    return 1


def read(repo: Path, relative: str) -> str:
    return (repo / relative).read_text(encoding="utf-8")


def main() -> int:
    repo = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
    failures = 0

    walk = read(repo, "src/MeshCraft/Application/WalkMode.cpp")
    file_ops = read(repo, "src/MeshCraft/Application/FileOps.cpp")
    overlays = read(repo, "src/MeshCraft/Application/UI/Overlays.cpp")
    pre_js = read(repo, "cmake/web/pre.js")
    cmake = read(repo, "CMakeLists.txt")
    rotation_algorithms = read(repo, "include/MeshCraft/RotationConventionAlgorithms.hpp")
    math_utils = read(repo, "mc3togltf/src/MathUtils.hpp")
    mouse = read(repo, "src/MeshCraft/Application/Mouse.cpp")
    event_preview = read(repo, "include/MeshCraft/Editor/EventPreviewRunner.hpp")
    event_preview_app = read(repo, "src/MeshCraft/Application/EventPreview.cpp")
    renderer = read(repo, "src/MeshCraft/Renderer/SceneRenderer.cpp")
    readme = read(repo, "README.md")
    format_doc = read(repo, "MC3_FORMAT.md")
    testing = read(repo, "TESTING.md")
    matrix = read(repo, "docs/CAPABILITY_MATRIX.md")

    for proxy in ('obj.collision == "box"', 'obj.collision == "sphere"',
                  'obj.collision == "capsule"'):
        failures += require(walk, proxy, f"Walk Mode dispatches {proxy}")
    failures += require(file_ops, "recoverFromAutosave()", "autosave recovery action exists")
    failures += require(file_ops, "discardAutosave()", "autosave discard action exists")
    failures += require(overlays, "Recover Unsaved Changes##recoverdlg",
                        "autosave recovery modal exists")
    failures += require(pre_js, "FS.mount(IDBFS, {}, '/home/web_user')",
                        "web bootstrap mounts IDBFS")
    failures += require(pre_js, "FS.syncfs(true", "web bootstrap restores IDBFS")
    failures += require(pre_js, "FS.syncfs(false", "web bootstrap persists IDBFS")
    failures += require(cmake, "-lidbfs.js", "web build links IDBFS support")
    failures += require(rotation_algorithms, "normalisedEulerOrderAlg",
                        "shared rotation helper supports Euler order")
    failures += require(rotation_algorithms, "rotationAsDegreesXYZAlg",
                        "shared rotation helper supports explicit normalization")
    failures += require(math_utils, "MeshCraft::rotationQuaternionAlg",
                        "glTF exporter uses the shared rotation helper")
    failures += require(mouse, "document_.rotationUnits, document_.eulerOrder",
                        "viewport picking passes document rotation convention")
    failures += require(renderer, "rotationMatrixForDocumentAlg",
                        "renderer uses document rotation convention")
    failures += require(event_preview, "updateAreaTransitions",
                        "Preview runner implements Area enter/exit transitions")
    failures += require(event_preview, "advanceTimers",
                        "Preview runner implements bounded timers")
    failures += require(event_preview, "documentCommitted",
                        "Preview runner exposes atomic commit outcome")
    failures += require(mouse, "automationWorkspace_.previewEnabled && bestObj",
                        "viewport click dispatch is gated by explicit Preview mode")
    failures += require(event_preview_app, "Preview event rolled back",
                        "editor reports failed Preview transaction rollback")

    for target, phrase, description in (
        (matrix, "box`, uniform `sphere`/IcoSphere, and compatible upright `capsule`",
         "matrix documents supported Walk Mode proxy classes"),
        (matrix, "Recover Unsaved Changes", "matrix documents the recovery modal"),
        (matrix, "mounts IDBFS", "matrix documents web persistence bootstrap"),
        (matrix, "Default, box, and sphere projection are previewed",
         "matrix documents ordinary-object viewport UV mapping"),
        (matrix, "animated transforms honor the authored convention",
         "matrix documents live rotation-convention support"),
        (matrix, "Explicit Preview/Play executes timer, Walk Mode Area enter/exit, and picked-object click bindings",
         "matrix documents bounded Event Preview/Play support"),
        (readme, "docs/CAPABILITY_MATRIX.md", "README links to the capability matrix"),
        (readme, "Recover Unsaved Changes", "README documents the recovery modal"),
        (readme, "collision=\"sphere\"`/IcoSphere", "README documents supported proxies"),
        (format_doc, "normal/UV geometry", "format documentation describes viewport UV support"),
        (format_doc, "bakes static object, state, definition and camera rotations",
         "format documentation describes safe rotation normalization"),
        (format_doc, "Normal editing does not dispatch bindings",
         "format documentation separates Preview from ordinary editing"),
        (testing, "shared rotation-convention path", "testing guide registers the documentation check"),
    ):
        failures += require(target, phrase, description)

    failures += forbid(readme, "Other collision proxy labels remain",
                       "README no longer claims box-only Walk Mode collision")
    failures += forbid(readme, "FS.mount`/`FS.syncfs` are never called",
                       "README no longer claims absent IDBFS calls")
    failures += forbid(readme, "this is a **notification only**",
                       "README no longer claims recovery is notification-only")
    failures += forbid(format_doc, "SceneRenderer.cpp` never reads it",
                       "format documentation no longer claims UV mapping is exporter-only")
    failures += forbid(matrix, "Not yet fully honored",
                       "matrix no longer claims the rotation-convention gap")
    failures += forbid(format_doc, "The live editor still assumes degrees",
                       "format documentation no longer claims degrees/XYZ-only editor behavior")
    failures += forbid(readme, "provides dry-run enter/exit/click/timer simulation",
                       "README no longer claims bindings are dry-run only")
    failures += forbid(format_doc, "viewport picking does not yet generate live",
                       "format documentation no longer claims click events are absent")

    if failures:
        print(f"{failures} capability-documentation check(s) failed.", file=sys.stderr)
        return 1
    print("All capability-documentation checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
