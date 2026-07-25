#!/usr/bin/env python3
"""Guard MeshCraft's ImGui production path against native OpenGL regressions."""

from pathlib import Path
import sys


FORBIDDEN = (
    "ImGui_ImplOpenGL",
    "imgui_impl_opengl",
    "SDL_GL_",
    "GetColorGLHandle",
    "IMGUI_IMPL_OPENGL",
)


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    files = [root / "CMakeLists.txt"]
    files.extend((root / "include" / "MeshCraft").rglob("*.hpp"))
    files.extend((root / "src" / "MeshCraft").rglob("*.cpp"))

    violations = []
    for path in files:
        text = path.read_text(encoding="utf-8")
        for forbidden in FORBIDDEN:
            if forbidden in text:
                violations.append(f"{path.relative_to(root)}: {forbidden}")

    renderer = (root / "src/MeshCraft/ImGuiRenderer.cpp").read_text(encoding="utf-8")
    required = ("ImGui_ImplSDL3_InitForOther", "DrawUserIndexedPrimitives", "setScissorRectangleProperty")
    for capability in required:
        if capability not in renderer:
            violations.append(f"src/MeshCraft/ImGuiRenderer.cpp: missing CNA renderer capability {capability}")

    if violations:
        print("FAIL: ImGui renderer portability guard found:")
        for violation in violations:
            print(f"  {violation}")
        return 1
    print("PASS: MeshCraft ImGui rendering uses CNA and no native OpenGL path.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
