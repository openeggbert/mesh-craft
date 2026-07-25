#!/usr/bin/env python3
"""STAB-0719 -- heuristic scanner for editor UI mutations missing pushUndo().

MeshCraft's undo system is a whole-document-snapshot model (pushUndo()
deep-copies document_ before a mutation, not a per-operation Command
pattern) -- coverage depends entirely on every mutation call site
remembering to call it first. This script grep/brace-matches every ImGui
widget call that returns true on user interaction (Checkbox/Slider*/
DragFloat*/InputText*/Combo/ColorEdit*/RadioButton) and flags any whose
enclosing block (or immediately-following lines, to catch the
IsItemActivated()-gated pattern used for continuous-fire widgets like
sliders/color pickers) has no pushUndo()/ctx.pushUndo() call.

This is a HEURISTIC, not a strict linter -- do not wire it into CI as a
hard pass/fail gate. Most candidates it reports are false positives: local
dialog-buffer state (e.g. a file path being typed before "Export"),
tool/preference parameters (snap settings, scatter-tool ranges, AI
settings) that legitimately don't touch the document, or widgets whose
actual document mutation happens in a separate function called later. Each
candidate needs a human to read the surrounding code and judge whether it's
a real document-mutation gap or one of those.

Run periodically (e.g. before a release, or after adding a new Properties
panel section) and manually triage the output -- do not assume every line
printed is a bug. STAB-0719 (2026-07-10) ran this, manually triaged all 95
candidates found at the time, and confirmed + fixed 3 genuine clusters this
way (Extrude smooth/caps checkboxes, the ~14-field inline Material editor,
5 Environment/Fog fields) -- the rest were confirmed false positives by
inspection.

Usage: undo_coverage_audit.py <repo-root>
"""
import os
import re
import sys

FILES = [
    "src/MeshCraft/Scene/PropertiesPanel.cpp",
    "src/MeshCraft/Application/Animation.cpp",
    "src/MeshCraft/Application/Commands.cpp",
    "src/MeshCraft/Application/UI/LeftPanel.cpp",
    "src/MeshCraft/Application/UI/Overlays.cpp",
    "src/MeshCraft/Application/UI/MenuBar.cpp",
    "src/MeshCraft/Application/UI/Properties.cpp",
    "src/MeshCraft/Application/UI/Toolbar.cpp",
    "src/MeshCraft/Application/UI/Registry.cpp",
    "src/MeshCraft/Application/UI/Ai.cpp",
    "src/MeshCraft/Scene/SceneHierarchyPanel.cpp",
]

# Widgets that mutate state when they return true.
MUTATORS = re.compile(
    r'ImGui::(Checkbox|SliderFloat\d?|SliderInt\d?|DragFloat\d?|DragInt\d?|'
    r'InputText(Multiline)?|Combo|ColorEdit[34]|RadioButton|InputFloat\d?|InputInt\d?)\s*\('
)
# AUD-036c: also recognize ctx.undoOnActivate(...)/undoOnActivate(...) as an
# undo-equivalent call. undoOnActivate() (MeshCraftApplication_Commands.cpp,
# exposed on PropertiesContext as ctx.undoOnActivate) is a thin wrapper that
# calls pushUndo() internally on the activation frame and forwards the
# widget's changed-bool -- it collapses the
# `bool ch = Widget(...); if (IsItemActivated()) pushUndo(); if (ch) {...}`
# pattern into `if (ctx.undoOnActivate(Widget(...))) {...}` (see AUD-036b,
# commit 89d874d). Before this fix, the 3 call sites in PropertiesPanel.cpp
# that adopted ctx.undoOnActivate() (the ##pos/##rot/##scl DragFloat3 fields)
# were misreported as candidates even though they correctly capture undo
# state, because the literal substring "pushUndo(" never appears at their
# call site -- it's inside undoOnActivate()'s own definition instead.
UNDO_CALL = re.compile(r'\b(ctx\.)?(pushUndo|undoOnActivate)\s*\(')


def enclosing_block(lines, start_idx, window=40):
    """Crude brace matcher: collects lines from start_idx until braces balance."""
    depth = 0
    started = False
    out = []
    for i in range(start_idx, min(start_idx + window, len(lines))):
        line = lines[i]
        out.append(line)
        depth += line.count("{") - line.count("}")
        if "{" in line:
            started = True
        if started and depth <= 0:
            break
    return out


def scan(root):
    total_checked = 0
    candidates = []
    for rel in FILES:
        path = os.path.join(root, rel)
        if not os.path.exists(path):
            continue
        with open(path) as f:
            lines = f.readlines()
        for i, line in enumerate(lines):
            if not MUTATORS.search(line):
                continue
            total_checked += 1
            window = "".join(enclosing_block(lines, i))
            # Also check a few lines after, for the IsItemActivated()-gated
            # pushUndo() pattern used by continuous-fire widgets.
            tail = "".join(lines[i:min(i + 8, len(lines))])
            if UNDO_CALL.search(window) or UNDO_CALL.search(tail):
                continue
            candidates.append((rel, i + 1, line.strip()))
    return total_checked, candidates


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <repo-root>", file=sys.stderr)
        sys.exit(1)

    total, candidates = scan(sys.argv[1])
    print(f"Checked {total} mutator-widget call sites across {len(FILES)} files")
    print(f"Candidates with no nearby pushUndo() found: {len(candidates)}")
    print("(most are expected false positives -- see this script's own docstring "
          "for why; triage manually, do not treat this list as bugs)\n")
    for rel, ln, text in candidates:
        print(f"{rel}:{ln}: {text}")
