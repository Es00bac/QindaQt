# Handoff: Task-list T0 injected-facts source/static candidate

- **Worker:** Talia Grant (GLM `zai-coding-plan/glm-5.3-flash`, high)
- **Posted:** 2026-08-28T14:03:59Z
- **Candidate commit:** `4d70dc8` — "Add injected-facts task-list source
  model" on branch `worker/task-list-t0`, exact base `9db68c4`
  (`9db68c4023257b49421101fa1b13c73bbc2cfa85`), single parent, clean tree
  (zero uncommitted paths in the source worktree at posting time).

## Changed paths (all new except one additive nav edit)

- `src/shell/task_list/**` — pure Qt Core model: immutable
  `TaskWindowFact`/`TaskEntry`/`TaskGeneration` values with bounds (4,096
  windows, 64 workspaces per window, 512-char identities), batch-atomic
  hostile validation, deterministic grouping/ordering by application and
  QindaQt container (primary-collapse, sorted membership), per-output/
  workspace scope filtering, loading/empty/ready/degraded presentation with
  1-based keyboard identities and deterministic accessible names, and
  activate/minimize/close intent arbitration with fixed-order stale-id
  rejection (InvalidRequest → NoGeneration → SourceDegraded → StaleRevision
  → UnknownTask).
- `tests/shell/task_list/**` — six Qt Test suites (values, grouping,
  hostile validation, intents, scope filter, presentation) plus
  `check_task_list_boundary.cmake` policy test. The test `CMakeLists.txt`
  is standalone-configurable (mirrors `tests/shell_visibility`) but is
  deliberately NOT wired into `tests/CMakeLists.txt`, `src/CMakeLists.txt`,
  or `src/shell/CMakeLists.txt`.
- `docs/wiki/shell/task-list.md` — primary page;
  `docs/wiki/adr/0026-inject-task-list-facts-into-the-shell.md` — decision
  record; `mkdocs.yml` — two-line additive nav entry only.

## Verification actually run (source/static lane; no compiler/runtime)

- `./tools/check-source-shape` — pass (1,024 files checked, no violations;
  largest new file is 219 raw lines).
- `./tools/validate-docs` — pass (65 Markdown documents, nav valid).
- `python -m mkdocs build --strict` — pass (fresh isolated venv, mkdocs;
  site built in 0.62 s with zero warnings).
- `git diff --check` plus a trailing-whitespace/tab scan of every new file —
  clean.
- Manual Qt API/warning review pass (repo builds with `-Wconversion
  -Wsign-conversion`); two real defects were caught and repaired pre-commit:
  a `QSet::insert().value` freshness idiom that does not exist in Qt, and a
  missing `TaskListSourceStatus` include (enum moved to `task_list_types.h`).

Not run, by lane constraint: configure, compile, CTest, runtime, GUI,
session, or host-state work. No runtime behavior is claimed.

## Bounded caveats

- Test suites are unexercised by a compiler; they are hostile-coverage
  complete by construction but the exact review should confirm they compile
  before integration.
- The module CMakeLists references `qindaqt_enable_warnings` and expects the
  parent build or the standalone test preamble to include
  `cmake/QindaQtCompiler.cmake` first (same pattern as
  `src/services/power_protocol`).
- `docs/wiki/index.md`, `docs/wiki/architecture/module-boundaries.md`, and
  build wiring are untouched by ownership: the integrator should add the
  index link, the `src/shell/task_list` boundary row, and one
  `add_subdirectory()` per build root at integration.

## Requested next action

Independent exact review of `4d70dc8` by a different worker (contract
fidelity of the stale-id rejection order and container-collapse rules
deserve the closest read), then manager integration with the three shared
wiring follow-ups listed above. This lane is closed for edits while review
is pending; I remain reachable in this thread.
