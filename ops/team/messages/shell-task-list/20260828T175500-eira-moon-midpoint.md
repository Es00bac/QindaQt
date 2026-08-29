---
author: Eira Moon
timestamp: 2026-08-28T17:55:00Z
topic: shell-task-list
type: midpoint
---

# Midpoint: Task List T0 compiled repair

## Compile blockers

All three Astra Quill P0 findings are repaired in `/home/cabewse/work_SPaC3/container-wm-workers/task-list-t0-repair-eira`:

1. `tests/shell/task_list/tst_task_list_source_grouping.cpp:81-83` — parenthesized the `QStringList` initializer-list argument to `QCOMPARE`.
2. `tests/shell/task_list/tst_task_list_intents.cpp:73-75` — parenthesized the `QStringList` initializer-list argument to `QCOMPARE`.
3. `tests/shell/task_list/tst_task_list_source_validation.cpp:21` — restored the stale `TaskGeneration::ok()` expectation by returning `TaskListEvaluation` from `TaskListTest::publish()` and checking `evaluation.ok()`.

## Genuine defects found and fixed

Running the seven focused rows exposed four real defects beyond the compile blockers:

1. **Source defect — container primary omitted from `memberWindowIds`**: `src/shell/task_list/src/task_list_grouping.cpp` never seeded the primary window into the container's `memberWindowIds`, so `windowCount` was one short and intent adapters would not receive the primary as a target. Fixed by initializing `entry.memberWindowIds = {fact.windowId}` for `ContainerPrimary`.
2. **Test defect — duplicate window id in determinism test**: `tst_task_list_source_grouping.cpp` reused `"w-a"` as both a standalone id and a container primary window id, which validation correctly rejects as `DuplicateWindowId`. Changed the primary window id to `"w-p"`.
3. **Test defect — wrong output-scope expectation**: `tst_task_list_scope_filter.cpp` expected two entries for `output-2`, but only `w-out2` is assigned to that output. The all-workspaces fact is still on `output-1`. Corrected expectation to one entry.
4. **Test defect — stale `minimized` flag in singleton container name test**: `tst_task_list_presentation.cpp` copied a minimized container and only changed `windowCount` to 1, then expected the accessible name to omit the state suffix. State suffixes are intentional; cleared `minimized` for the singleton case so the test verifies only the window-count omission.
5. **Boundary test config**: `tests/shell/task_list/CMakeLists.txt` passed `CMAKE_SOURCE_DIR` as `SOURCE_ROOT`, which is wrong in standalone configure. Added conditional logic to use the project root in both integrated and standalone modes.

## Regression coverage added

- `TaskListSourceGroupingTest::primaryOnlyContainerCountsItself()` — a container with no extra members must still report `windowCount == 1` and `memberWindowIds == [primary]`.
- `TaskListIntentsTest::activationTargetsTheContainerPrimary()` — intent on a primary-only container resolves to the primary and enumerates it as the sole member target.

## Verification status

- Strict-warning Debug focused build: pass
- Strict-warning Release focused build: pass
- CTest 7/7: pass in both configurations
- Direct QtTest 6/6 executables: pass in Debug
- `./tools/check-source-shape`: pass
- `./tools/validate-docs`: pass
- `python3 -m mkdocs build --strict` (isolated venv): pass
- `git diff --check` / trailing-whitespace/tab scan: clean

Next step: final review of the diff, commit, and hand off for exact non-Kimi review.
