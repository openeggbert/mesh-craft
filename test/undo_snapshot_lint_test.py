#!/usr/bin/env python3
"""Lint guard against the dead undo-snapshot pattern.

The editor used to nest the undo snapshot INSIDE a Drag*/ColorEdit* widget's
changed-block:

    if (ImGui::DragFloat3("##pos", pos, 0.1f)) {
        if (ImGui::IsItemActivated()) pushUndo();   // <-- DEAD
        ...apply edit...
    }

ImGui::DragBehaviorT force-returns false on the frame the widget is first
activated, and IsItemActivated() is true ONLY on that activation frame, so the
snapshot line never runs. The result was silent data loss: dragging a
transform/dimension/color/keyframe value was neither undoable nor did it
invalidate the redo stack. The fix evaluates IsItemActivated() every frame:

    bool ch = ImGui::DragFloat3("##pos", pos, 0.1f);
    if (ImGui::IsItemActivated()) pushUndo();
    if (ch) { ...apply edit... }

This test fails if the dead pattern is reintroduced anywhere in the editor UI:
an `if (ImGui::IsItemActivated()) [ctx.]pushUndo();` whose immediately-preceding
non-empty line ends with `{` and belongs to a Drag*/ColorEdit*/Input* widget.
Slider* widgets are exempt — they can return true on the activation-click frame,
so the nested snapshot there does fire.

Usage: undo_snapshot_lint_test.py <src-root>
"""
import os
import re
import sys

BUGGY = ("DragFloat", "DragInt", "ColorEdit", "ColorPicker",
         "InputFloat", "InputInt", "InputText", "InputDouble")
ACT_RE = re.compile(r'^\s*if \(ImGui::IsItemActivated\(\)\)\s*(?:ctx\.)?pushUndo\(\);\s*$')

FILES = [
    "src/MeshCraft/Scene/PropertiesPanel.cpp",
    "src/MeshCraft/Application/UI/LeftPanel.cpp",
    "src/MeshCraft/Application/Animation.cpp",
]


def widget_of_block(lines, act_idx):
    """If the block containing act_idx opens with a buggy changed-widget, return
    its name; else None. Walks back over the `{`-terminated opener."""
    j = act_idx - 1
    while j >= 0 and lines[j].strip() == "":
        j -= 1
    if j < 0 or not lines[j].rstrip().endswith("{"):
        return None
    # Walk back to the line holding `if (ImGui::<Widget>(`
    k = j
    while k >= 0 and "if (ImGui::" not in lines[k]:
        k -= 1
        if act_idx - k > 6:   # give up: not a simple widget opener
            return None
    if k < 0:
        return None
    m = re.search(r'if \(ImGui::([A-Za-z0-9_]+)\(', lines[k])
    return m.group(1) if m else None


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    dead = []
    for rel in FILES:
        path = os.path.join(root, rel)
        if not os.path.exists(path):
            print(f"SKIP (not found): {rel}")
            continue
        lines = open(path).read().split("\n")
        for i, l in enumerate(lines):
            if not ACT_RE.match(l):
                continue
            w = widget_of_block(lines, i)
            if w and any(w.startswith(b) for b in BUGGY):
                dead.append(f"{rel}:{i + 1}  (inside a changed-block of ImGui::{w})")

    if dead:
        print("FAIL: dead undo-snapshot pattern found (undo will silently not fire):")
        for d in dead:
            print("  " + d)
        sys.exit(1)
    print("PASS: no dead undo-snapshot pattern in the editor UI.")


if __name__ == "__main__":
    main()
