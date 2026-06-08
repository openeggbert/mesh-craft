# NEXT.md

> Full feature plan and architecture reference are in **PLAN.md**.

---

## Recently completed

- Object rename — click name bar in properties panel, Enter/Esc
- Status bar text — object count and selection count
- Undo/Redo — Ctrl+Z/Y, 20 steps, deep-copy document snapshots
- Scale gizmo (S) — flat-square tips, drag scales along axis
- Rotate gizmo (R) — 3 coloured circles (XYZ), click circle to start drag, tangential mouse movement rotates
- Properties: Visible toggle (VIS row, click to flip on/off) + Collision field (COL row, click to edit string)
- Cut/Copy/Paste — Ctrl+X/C/V; clipboard survives selection changes; paste offsets +1 X
- Hierarchy visibility eye icon — click to toggle visible per row; hidden objects shown dimmed
- Properties Tags field — click TAG row to edit comma-separated tags; Enter applies, Esc cancels
- Group/Ungroup — Ctrl+G wraps selection into a Group; Ctrl+Shift+G dissolves Group back to parent
- Preset views — Num1/3/5/7/9 for Front/Right/Back/Top/Bottom camera angles
- Box drag-select — drag in 3D viewport (Select tool) draws blue rectangle; release selects enclosed objects; Ctrl = additive
- Open file dialog — Ctrl+O shows centered in-UI modal; type path, Enter loads, Esc cancels; error shown in red
- Save As dialog — Ctrl+Shift+S reuses same modal; pre-fills current path; appends .mc3.xml if missing

---

## In progress

_(nothing)_

---

## Next up

1. **Material editor** — create / rename / delete materials; edit baseColor RGBA sliders
2. **Drag-and-drop reparenting** — drag a hierarchy row onto another to reparent
3. **Recent files list** — remember last N opened files, show in a dropdown or panel

---

## Resume prompt

```
Read PLAN.md and NEXT.md. Implement the next task from "Next up".
Do not refactor unrelated code. Build: cd cmake-build-debug && ninja -j$(nproc).
Test: ctest --test-dir cmake-build-debug -V. Update NEXT.md when done.
```
