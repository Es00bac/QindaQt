# Display D3 Client hostile-test claim

## Outcome

Develop focused hostile QtTest sources exercising the production public Display1 client/coordinator boundary with A/B/A owner lineage, epoch/revision rollback, timeout and late reply, atomic snapshot publication, preview/confirm/revert/cancel state machine, busy/degraded/unavailable service states, and teardown lifecycle. Tests must fail on missing behavior and require no fake implementation.

## Base and worktree

- Base commit: `ab36cd8` (public `main`)
- Branch: `worker/display-d3-kimi-nyra`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra`

## Paths and ownership

- Test ownership: `tests/services/display_client/**` (exclusive)
- No production edits, docs, module CMake, git index, commits, or Pavel/Nyra files

## Completion evidence

- Focused QtTest binaries in `tests/services/display_client/` exercising client state machine, owner A/B/A lineage rejection, transport timeout/late-reply handling, snapshot publication atomicity, transaction operation ordering (stage→preview→confirm/cancel), and service unavailable/degraded/busy states
- Client tests only run after Pavel registers targets; commands and test counts reported at handoff
- No commit, no host display/input, generated builds kept outside worktree
- Boundaries: exercise production Client and QtDisplayTransport only; do not mock or patch coordinator/service internals

## Collision and dependency risks

- Depends on Pavel's Display1 D3 protocol/service completion and exact public headers stability in `src/services/display_client/**`
- No collision risk with D2 service tests; coordinate with Nyra if Display transport seams change

## Next action

Pavel: confirm exact test coverage scope, any known protocol gaps, and authorize test target registration when ready. Tara: create support fixtures and write first test case for A/B/A epoch-reuse rejection, then report midpoint.
