---
author: Eira Moon
timestamp: 2026-08-28T18:10:00Z
topic: shell-task-list
type: handoff
---

# Handoff: Task List T0 compiled-repair candidate

## Candidate

- **Commit:** `dc1f36ebd4506e005f666cc1fef2fcb03673d684`
- **Tree:** `22aa2daa0622f62e850e5fb87e2e050029ee25b6`
- **Parent:** `4d70dc8e8be9c6e0bed16052b8a00729afe7ce6d`
- **Branch:** `worker/task-list-t0-repair-eira`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/task-list-t0-repair-eira`
- **State:** clean tree, no uncommitted product paths

## Changed paths (exclusive to this lane)

- `src/shell/task_list/src/task_list_grouping.cpp` — source fix: include the container primary in `memberWindowIds`.
- `tests/shell/task_list/CMakeLists.txt` — boundary-test `SOURCE_ROOT` correction for standalone configure.
- `tests/shell/task_list/task_list_test_support.h` — `publish()` returns `TaskListEvaluation`.
- `tests/shell/task_list/tst_task_list_source_grouping.cpp` — `QCOMPARE` parenthesization, duplicate-window-id test fix, `QVERIFY(evaluation.ok())` throughout, new `primaryOnlyContainerCountsItself` regression test.
- `tests/shell/task_list/tst_task_list_intents.cpp` — `QCOMPARE` parenthesization, `QVERIFY(publish().ok())`, new `activationTargetsTheContainerPrimary` regression test.
- `tests/shell/task_list/tst_task_list_scope_filter.cpp` — corrected output-scope expectation, `Q_ASSERT` in helper.
- `tests/shell/task_list/tst_task_list_presentation.cpp` — `QVERIFY(evaluation.ok())`, clear stale `minimized` flag in singleton-container name test.
- `tests/shell/task_list/tst_task_list_source_validation.cpp` — operate on `TaskListEvaluation` returned by `publish()`.

## Verification

| Gate | Command | Result |
|---|---|---|
| Strict-warning Debug configure/build | `cmake -S tests/shell/task_list -B build-tests-debug -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_ENABLE_STRICT_WARNINGS=ON && cmake --build build-tests-debug` | Pass |
| Debug CTest | `ctest --output-on-failure` | 7/7 pass |
| Direct Debug QtTest | each `./qindaqt_task_list_*_tests` | 6/6 pass |
| Strict-warning Release configure/build | `cmake -S tests/shell/task_list -B build-tests-release -DCMAKE_BUILD_TYPE=Release -DQINDAQT_ENABLE_STRICT_WARNINGS=ON && cmake --build build-tests-release` | Pass |
| Release CTest | `ctest --output-on-failure` | 7/7 pass |
| Source shape | `./tools/check-source-shape` | Pass (1,024 files) |
| Docs validation | `./tools/validate-docs` | Pass (65 docs) |
| Strict MkDocs | `python3 -m venv /tmp/mkdocs-venv-task-list && .../bin/python -m mkdocs build --strict -d /tmp/mkdocs-build-task-list` | Pass |
| Whitespace | `git diff --check`; trailing-whitespace/tab scan | Clean |
| Clean tree | `git status` | Nothing to commit |
| Provenance | `git log -1 --format='%H %T %P'` | `dc1f36e... 22aa2da... 4d70dc8...` |

## Defects repaired

1. **Source defect:** container primary omitted from `memberWindowIds` — `windowCount` and intent member targets now include the representative primary plus every suppressed member.
2. **Test defect:** duplicate window id in determinism test.
3. **Test defect:** incorrect output-scope expectation.
4. **Test defect:** stale `minimized` flag in singleton-container accessible-name test.
5. **Boundary-test defect:** `SOURCE_ROOT` pointed at `tests/shell/task_list` in standalone configure.

## Contracts preserved

- Injected compositor-fact ownership (no KWin/platform/DBus/GUI dependencies).
- Container grouping (one primary per container, suppressed members collapsed).
- Activation intents (fixed rejection order, stale-revision-before-unknown-task).
- Scope/filter (empty axis means unrestricted; all-workspaces participates in every workspace).
- Accessibility (deterministic application/title/count/state composition).
- Determinism (canonical order independent of producer fact order).
- Failure contracts (batch-atomic validation, rejected publish leaves retained generation untouched).

## Caveats

- The module remains deliberately unwired to `src/CMakeLists.txt`, `tests/CMakeLists.txt`, and `src/shell/CMakeLists.txt` per the T0 lane boundary; the integrating manager must add the single `add_subdirectory()` line in each parent build.
- `docs/wiki/index.md` and `docs/wiki/architecture/module-boundaries.md` were untouched by ownership; the integrator should add the index link and boundary row.
- No GUI, session, host desktop, or runtime qualification is claimed; this is the pure source/static slice.

## Requested next action

Exact independent review by a non-Kimi worker. Reviewers should pay closest attention to:
- Container primary inclusion in `memberWindowIds` and the deterministic sorting/order contract.
- Stale-id rejection order (`InvalidRequest → NoGeneration → SourceDegraded → StaleRevision → UnknownTask`).
- Boundary-test `SOURCE_ROOT` behavior in both integrated and standalone configure.
