---
author: Astra Quill
timestamp: 2026-08-28T10:56:01-06:00
topic: shell-task-list
type: review
status: FAIL
---

# Review: Task List T0 - FAIL

**Reviewer**: Astra Quill
**Candidate**: `4d70dc8e8be9c6e0bed16052b8a00729afe7ce6d`
**Tree**: `722a41d94718a18178934aeef3fc24992ec8bccc`
**Parent**: `9db68c4023257b49421101fa1b13c73bbc2cfa85`

## Verdict: FAIL

The candidate fails because the test suites do not compile.

**Defect Counts**:
- P0: 3 (Build/Compilation failures)
- P1: 0
- P2: 0
- P3: 0

### P0 Findings:
1. `tests/shell/task_list/tst_task_list_source_grouping.cpp:81-83`: `QCOMPARE` macro passed 4 arguments instead of 2. An initializer list with commas `QStringList{..., ..., ...}` must be wrapped in parentheses or bound to a variable before passing to `QCOMPARE`.
2. `tests/shell/task_list/tst_task_list_intents.cpp:73-75`: Similar `QCOMPARE` macro syntax error due to an unparenthesized initializer list.
3. `tests/shell/task_list/tst_task_list_source_validation.cpp:21`: `TaskGeneration::ok()` is called but `const struct QindaQt::ShellTaskList::TaskGeneration` has no member named `ok`.

## Verification Executed

- `./tools/check-source-shape`: Pass (exit 0)
- `./tools/validate-docs`: Pass (exit 0)
- `python3 -m mkdocs build --strict -d /tmp/mkdocs-build`: Pass (exit 0, built in 0.59s)
- `git diff --check` / whitespace scan: Clean (no trailing whitespace)
- **Out-of-tree CMake configure**: Pass (exit 0)
  ```bash
  cmake -S tests/shell/task_list -B ../build_tests
  ```
- **Out-of-tree Test Build**: Fail (exit 2)
  ```bash
  cmake --build ../build_tests -- -k
  ```
  Result: `qindaqt_task_list_values_tests`, `qindaqt_task_list_scope_filter_tests`, and `qindaqt_task_list_presentation_tests` successfully built. `grouping`, `validation`, and `intents` tests failed to compile.
- **CTest**: Skipped due to build failure.

## Bounded Caveats
- No tests were actually run because the required binaries failed to compile. The byte-clean worktree was verified after out-of-tree steps.

## Requested Next Action
Implementer (Talia Grant) must repair the compilation errors in the test suites within the existing worktree. Do not change the module behavior just to make the tests pass; ensure the tests correctly align with the types defined in `task_list_types.h`. Once repaired, provide a new candidate commit for review.
