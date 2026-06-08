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

---

## In progress

_(nothing)_

---

## Next up

1. **Cut/Copy/Paste** — Ctrl+X/C/V for scene objects
2. **Hierarchy visibility toggle** — eye icon per row to toggle visible without selecting
3. **Properties: Tags list** — display/edit `Mc3Object::tags` (comma-separated)

---

## Resume prompt

```
Read PLAN.md and NEXT.md. Implement the next task from "Next up".
Do not refactor unrelated code. Build: cd cmake-build-debug && ninja -j$(nproc).
Test: ctest --test-dir cmake-build-debug -V. Update NEXT.md when done.
```
