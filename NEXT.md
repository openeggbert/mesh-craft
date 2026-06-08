# NEXT.md

> Full feature plan and architecture reference are in **PLAN.md**.

---

## Recently completed

- Object rename — click name bar in properties panel, Enter/Esc
- Status bar text — object count and selection count
- Undo/Redo — Ctrl+Z/Y, 20 steps, deep-copy document snapshots
- Scale gizmo (S) — flat-square tips, drag scales along axis
- Rotate gizmo (R) — 3 coloured circles (XYZ), click circle to start drag, tangential mouse movement rotates

---

## In progress

_(nothing)_

---

## Next up

1. **Properties panel: Visible toggle + Collision field** — add two small fields below the material swatch
2. **Cut/Copy/Paste** — Ctrl+X/C/V for scene objects

---

## Resume prompt

```
Read PLAN.md and NEXT.md. Implement the next task from "Next up".
Do not refactor unrelated code. Build: cd cmake-build-debug && ninja -j$(nproc).
Test: ctest --test-dir cmake-build-debug -V. Update NEXT.md when done.
```
