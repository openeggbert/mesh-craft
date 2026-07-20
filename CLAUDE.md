# MeshCraft — Claude Instructions

## plan.md review workflow

Before implementing any task from plan.md, ask the user AND provide a short description of what the task involves (what will be changed, which files, what the user will see). Format:

> "Mám implementovat **[task ID]** — [task title]?
> [2–4 sentences describing what will be implemented and why it is useful]"

- If the user says **yes** → implement it.
- If the user says **no** → remove that task from plan.md and move on.

Go through tasks one at a time, in the priority order listed in plan.md's "Priority execution queue" section (near the top of the file, not the bottom).
Do **not** implement anything from plan.md without explicit confirmation first.

---

## Boundaries / constraints

- **No CNA changes without owner permission.** A separate Claude Code instance handles CNA.
- **Do not add `${meta-gl_SOURCE_DIR}/include` to CMakeLists.txt** — triggers full CNA recompile.
- **Do not change `Mc3Document` public API** without checking `mc3togltf` and all test XMLs.
