# Nadia Park — exact applied-baseline dirty-truth repair handoff

- **Time:** 2026-08-28T11:19:49-06:00
- **Status:** finished; review requested; no longer working
- **Exact commit:** `e53a3505ec50a0819bbf0ccd4204d2926fe657fd`
- **Exact tree:** `e832ccb8dfe0352070276313f7635a04ba6668c1`
- **Exact parent:** `0bffed9c43701aebd7d39c9d31c98319573d6e8c`
- **Branch:** `worker/wysiwyg-c0-dirty-repair-nadia`
- **Preserved worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/wysiwyg-customization-c0-dirty-repair-nadia`
- **Finding closed:** sole P1 in Elion Brooks messages `1787936845` and
  `1787936908`; none of the fifteen previously closed findings were reopened
- **Requested next action:** Elion Brooks independently rereview exact commit
  `e53a3505ec50a0819bbf0ccd4204d2926fe657fd`, especially the two production
  cases in `qindaqt.customize-editor-dirty-state`

## Outcome

`EditorSession` now captures the constructor's committed profile as the
canonical applied schema-v1 baseline and initializes the applied profile
identity from that same snapshot. A successful atomic Apply replaces the
baseline; a failed Apply and Revert leave it and the existing dirty truth
unchanged. Successful point/drag commits, Undo, and Redo derive dirty state by
exact full-profile comparison. Revision and undo-stack position are no longer
used as content-difference proxies, and missing/provisional comparison state
fails dirty.

A dedicated production-composition suite uses the real
`LayoutEditingRepository`, `CoordinatorEditingEngine`, and profile store to
prove:

1. cross-panel edit → Undo restores the constructor profile and reports clean;
2. edit → Apply persists the exact new profile, Undo reports dirty against
   that new baseline, and Redo restores the applied profile and reports clean.

The prior 493-line session test was reduced rather than extended, preserving
the source-decomposition boundary and moving dirty-history behavior into one
cohesive failure-mode suite. Failed Apply and Revert coverage now also compose
the production repository adapter.

## Exact changed paths

- `docs/wiki/adr/0043-isolate-the-customization-editor-domain.md`
- `docs/wiki/shell/customization-editor.md`
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/editor_session.h`
- `src/shell_customization_editor/src/editor_session.cpp`
- `src/shell_customization_editor/src/editor_session_gestures.cpp`
- `tests/shell_customization_editor/CMakeLists.txt`
- `tests/shell_customization_editor/tst_editor_session.cpp`
- `tests/shell_customization_editor/tst_editor_session_dirty_state.cpp` (new)

## Verification on the exact commit

- Fresh dependency-light Debug configure with `BUILD_TESTING=ON`, production
  shell/KWin/shell disabled, and `CMAKE_COMPILE_WARNING_AS_ERROR=ON`: exit `0`.
- Actual editor compile commands contain `-Wall -Wextra -Wpedantic
  -Wconversion -Wsign-conversion -Wshadow -Werror`.
- Build of all six editor targets and eight adjacent profiles/transaction
  targets: initial **117/117 Ninja actions**, final exact-commit incremental
  build clean, exit `0`.
- `ctest --test-dir build/nadia-dirty --output-on-failure -j1 -R
  '^qindaqt\\.(customize-editor-|profile-|shell-customization-)'`:
  **14/14 passed**, exit `0`.
- `./tools/check-source-shape`: **1,032 source files passed**, exit `0`; the
  new dirty-state test is 81 nonblank lines and the reduced session test is
  460, both below the decomposition-review threshold.
- `./tools/validate-docs`: **65 Markdown documents plus navigation passed**,
  exit `0`.
- isolated MkDocs environment `mkdocs build --strict`: documentation built
  successfully, exit `0`.
- `git diff 0bffed9c..e53a350 --check`: exit `0`.
- `git status --porcelain`: empty; branch and preserved worktree are clean.

## Bounded caveat

The candidate remains the presentation-independent customization editor
domain. It does not claim the later QML canvas, Settings route/composition,
provisional shell binding, live shell behavior, or nested visual qualification.
No host GUI, compositor, session bus, input device, user configuration, shared
roadmap, metrics, or integration branch was touched.

— Nadia Park, exact repair handed off; not live.
