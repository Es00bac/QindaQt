# Claim: Task-list T0 bounded injected-facts source/static slice

- **Worker:** Talia Grant (GLM `zai-coding-plan/glm-5.3-flash`, high)
- **Posted:** 2026-08-28T13:45:42Z
- **Status:** Self-declared working from this live process.

## User-visible outcome

A bounded task-list foundation in
`/home/cabewse/work_SPaC3/container-wm-workers/task-list-t0`, branch
`worker/task-list-t0`, base `9db68c4023257b49421101fa1b13c73bbc2cfa85`
(re-verified: HEAD exact, clean tree): immutable window/task fact and entry
values, deterministic grouping/ordering by application and QindaQt container
(one container identity per collapsed group), active/urgent/minimized state,
activation/minimize/close request intents with stale-id rejection, per-output/
workspace filtering policy, loading/empty/degraded presentation, deterministic
keyboard and accessibility identities, and hostile tests for malformed,
duplicate, orphaned, and over-limit injected facts.

## Path ownership (exclusive)

- New `src/shell/task_list/**`
- New `tests/shell/task_list/**`
- New primary wiki page `docs/wiki/shell/task-list.md` and new ADR under
  `docs/wiki/adr/`; smallest additive `mkdocs.yml` nav entry required by the
  documentation policy.

## Completion evidence intended

Static/whitespace/source-shape/docs checks only. No configure, compile, CTest,
GUI, session, or runtime work; no runtime behavior claims at handoff.

## Constraints honored

Consume only public/injected window facts as plain values. No KWin private ABI,
no window management, no close/activation side effects, no host state, no
production shell edits, no build registries or shared CMake wiring
(`src/CMakeLists.txt`, `tests/CMakeLists.txt`, `src/shell/CMakeLists.txt`
remain untouched for the integrator), no roadmap edits. The worktree fallback
board is never used; this directory is the live board.

## Collision/dependency risks

None known. Verified `worker/appshell-s0`, `worker/launcher-l0`,
`worker/shell-surface-repair`, and the settings/appearance lanes touch disjoint
paths. `mkdocs.yml` is the only shared coordination point and receives a
two-line additive nav edit. Shared-doc follow-ups for the integrator (wiki
index link, module-boundaries row, CMake wiring) will be enumerated exactly in
the handoff.
