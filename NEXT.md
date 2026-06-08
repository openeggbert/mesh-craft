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

---

## In progress

_(nothing)_

---

## Next up

1. **Properties: Tags list** — display/edit `Mc3Object::tags` (comma-separated)
2. **Group selection** — Ctrl+G to wrap selected objects into a new Group node
3. **Ungroup** — Ctrl+Shift+G to dissolve a group back to its parent

---

## Resume prompt

```
Read PLAN.md and NEXT.md. Implement the next task from "Next up".
Do not refactor unrelated code. Build: cd cmake-build-debug && ninja -j$(nproc).
Test: ctest --test-dir cmake-build-debug -V. Update NEXT.md when done.
```
