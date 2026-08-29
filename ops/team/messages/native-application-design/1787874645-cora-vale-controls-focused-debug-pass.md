# Cora Vale checkpoint: Controls focused Debug gate passes

- **Timestamp:** 2026-08-27T23:50:45Z
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Branch / baseline:** `worker/controls-s2` / `a083a20af14a2d7b9e954735a2d659c475a536b2`
- **Scope:** repaired focused Controls build and tests only; no broad, visual, Release, or install work started

## Exact evidence

1. `git diff --check`
   - Exit 0.
2. `cmake --build build/controls-debug --target qindaqt_controls_behavior_tests qindaqt_controls_qml_qmllint --parallel 1`
   - Exit 0, 11/11 build actions.
   - `qindaqt_controls_qml_qmllint` emitted zero warnings.
3. `ctest --test-dir build/controls-debug -R '^qindaqt\.controls-(behavior|source-policy|pss-measurement)$' --output-on-failure --parallel 1`
   - Exit 0, 3/3 CTests passed.
4. `ctest --test-dir build/controls-debug -R '^qindaqt\.controls-behavior$' --verbose --parallel 1`
   - Exit 0; QtTest totals 19 passed, 0 failed, 0 skipped.
   - All five built-in theme behavior rows passed. No QML runtime warning or FormRow binding-loop warning appeared.

The final bounded repair uses the inherited `TextInput.contentHeight` contract for
`T.TextField` sizing and compares disabled token colors using the public 8-bit
`QColor::rgba()` representation. The compiler process is stopped and the lane was
returned to the manager immediately after this checkpoint.

## Next boundary

Visual baseline generation/review, full Debug install/broad qualification, Release,
and documentation gates remain. They will run sequentially only after compiler-lane
coordination. No candidate commit exists yet.
