# Cora Vale claim: Audio1 focused-selector documentation repair

- **Timestamp:** 2026-08-27T20:55:15Z
- **Worker:** Cora Vale — OpenAI Codex runtime; exact serving model and
  reasoning variant are not exposed and are not inferred
- **Manager coordination:** explicit temporary path transfer because the
  original implementer finished handoff and cannot be resumed within the live
  thread cap
- **Exact base/HEAD:** `bd3a94e32aff5a5bd8bde737aae62e8330241734`
- **Branch:** `worker/audio1-service`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/audio1-service`

## Bounded outcome and ownership

Audit every published Audio1 selector/count and make the smallest docs-only
repair so the canonical focused selector selects all seven current Audio tests,
including `qindaqt.audio-wireplumber-reset-lifecycle`, and no unrelated test.
Only affected Audio documentation is owned; runtime/test source, shared main,
TASK_LIST/HANDOFF, and unrelated paths remain untouched.

Evidence will use existing Debug/Release/sanitizer trees without rebuild:
exact `ctest -N` discovery, selected 7/7 in all three configurations, strict
docs/links/source/whitespace, and exact process/temp cleanup. The result will be
a new standalone commit requesting recheck from the same exact reviewer.
