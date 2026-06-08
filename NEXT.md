# NEXT.md

> Detailní plán funkcí a architektura jsou v **PLAN.md**.

---

## Nedávno dokončeno

- Object rename — klik na name bar v properties panelu, Enter/Esc
- Status bar text — počet objektů a výběrů
- Undo/Redo — Ctrl+Z/Y, 20 kroků, deep-copy snímků dokumentu
- Scale gizmo (S) — flat-square tipy, drag škáluje po ose

---

## Právě se dělá

_(nic)_

---

## Další na řadě

1. **Rotate gizmo (R)** — kruhové oblouky pro X/Y/Z, klik+drag = rotace

---

## Resume prompt

```
Přečti PLAN.md a NEXT.md. Implementuj další úkol z "Další na řadě".
Nerefaktoruj nesouvisející kód. Build: cd cmake-build-debug && ninja -j$(nproc).
Test: ctest --test-dir cmake-build-debug -V. Po dokončení aktualizuj NEXT.md.
```
