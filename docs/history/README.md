# Historical archive

These documents are **superseded snapshots**, kept for provenance only. They
are NOT current and must not be treated as authoritative. For live status see
the repository root:

- `plan.md` — the single active backlog.
- `NEXT.md` — the current baseline and handoff.
- `TESTING.md` — the registered test suite.

| File | Was | Superseded by |
|------|-----|---------------|
| `STABILIZATION.md` | The stabilization policy + Gates A-F quality-bar definitions. Moved here 2026-07-18 once the stabilization phase was substantively complete (all gates A-E satisfied per `plan.md`'s state; remaining backlog is owner-gated/deferred, not active defect work) — most of its own content (CNA/`Mc3Document`/meta-gl boundary rules) was already duplicated in `CLAUDE.md`, and its emoji status legend (✅🟡🧪📋🔴) no longer matched `plan.md`'s actual `[DONE]`/`[TODO]`/`[BLOCKED]`/`[DEFERRED]` notation | `CLAUDE.md` (boundary rules); `plan.md`/`NEXT.md` (live status). The Gates A-F table itself has no live replacement — cite this archived copy if a future audit needs to reference a specific gate |
| `plan_20260710.md` | The 723-row STAB stabilization plan | `plan.md` (evidence-based backlog) |
| `STABILIZATION_VERIFICATION.md` | A point-in-time verification snapshot (claimed Web/Emscripten ✅) | `NEXT.md` baseline (Web is a documented blocker) |
| `STABILIZATION_WORKLOG.md` | Running STAB worklog | Git history + `CHANGELOG.md` |
| `web_issues.md` | Early Web-build issue theories (sizing) | `NEXT.md` (crash root-cause) |
| `plan_stabilization_master.md` | The original 723-row STAB master plan (was the root `plan.md`) | `plan.md` (evidence-based backlog) |
| `plan_deep_audit.md` | The 2026-07-09 deep-audit follow-up plan; all 57 of its AUDIT-#### tasks are completed | `plan.md` (current backlog); code comments citing `AUDIT-####` IDs still point here for context |
| `plan_20260718.md` | Not a superseded plan — a **pruned snapshot**: the 61 `AUD-###` + 33 `SYS-###` rows that were `DONE` in `plan.md` as of 2026-07-18, moved out verbatim (full evidence/resolution text) to keep the active file down to just its still-open rows | `plan.md` (still tracks these ids in its "Net across all"/session-log tallies; code comments citing an archived id resolve here) |
| `AI_TRUNCATION_BUG.md` | A detailed bug report for AI Assistant responses being truncated mid-XML on complex prompts. Moved here 2026-07-19 — all 3 of its own proposed fixes (raise `max_tokens`, detect `stop_reason`/`wasTruncated()`, expose `max_tokens` in the UI) are implemented and live; it had been sitting at the repo root long after the fix landed, reading as an open bug | `AiAssistant.hpp`/`MeshCraftApplication_UiAi.cpp` (current implementation); `NEXT.md` (current status) |
