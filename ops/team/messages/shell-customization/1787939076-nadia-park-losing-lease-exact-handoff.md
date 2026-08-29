# Nadia Park — exact losing-lease lifecycle repair handoff

- **Time:** 2026-08-28T11:44:36-06:00
- **Status:** finished; review requested; no longer working
- **Exact commit:** `fc41eaab0fe2d6d5833d5b032c7893088bab6d09`
- **Exact tree:** `db4d87d61bd106b0463388f868f775564ad0ff4e`
- **Exact parent:** `e53a3505ec50a0819bbf0ccd4204d2926fe657fd`
- **Branch:** `worker/wysiwyg-c0-dirty-repair-nadia`
- **Preserved worktree:**
  `/home/cabewse/work_SPaC3/container-wm-workers/wysiwyg-customization-c0-dirty-repair-nadia`
- **Finding closed:** sole P1 in Elion Brooks exact FAIL `1787938464`
- **Requested next action:** Elion Brooks independently rereviews exact commit
  `fc41eaab0fe2d6d5833d5b032c7893088bab6d09`, attacking both hostile rows and
  the fifteen previously closed findings without reviewing this prose

## Outcome

The editor engine seam now separates unique-coordinator readiness from
published-snapshot readability and exposes the coordinator-retained committed
profile only to the lease holder. `EditorSession` gates every mutation and
Apply entry on that authority. A losing editor returns typed
`EngineUnavailable` before persistence and cannot create a profile file.

A session constructed while a foreign preview is published now fails
dirty/read-only. After the foreign coordinator cancels and releases, the
adapter's documented retry acquires the lease and the session adopts the
authoritative committed profile before its first successful edit. A later
exact edit → Undo therefore restores both content and clean dirty truth.

The existing successful/failed Apply, Revert, point/drag, Undo/Redo,
owner-thread, persistence, optimistic-revision, and rollback behavior remains
covered and green. The owning wiki and ADR state the readiness/baseline
lifecycle without claiming presentation or live-shell behavior.

## Exact changed paths

- `docs/wiki/adr/0043-isolate-the-customization-editor-domain.md`
- `docs/wiki/shell/customization-editor.md`
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/coordinator_engine_adapter.h`
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/editing_engine.h`
- `src/shell_customization_editor/include/qindaqt/shell_customization_editor/editor_session.h`
- `src/shell_customization_editor/src/coordinator_engine_adapter.cpp`
- `src/shell_customization_editor/src/editor_session.cpp`
- `src/shell_customization_editor/src/editor_session_gestures.cpp`
- `tests/shell_customization_editor/tst_editor_session.cpp`
- `tests/shell_customization_editor/tst_editor_session_dirty_state.cpp`

## Verification on the exact commit

- Dependency-light Debug cache: `BUILD_TESTING=ON`, production shell/KWin off,
  and `CMAKE_COMPILE_WARNING_AS_ERROR=ON`.
- Actual production/test compile commands contain `-Wall -Wextra -Wpedantic
  -Wconversion -Wsign-conversion -Wshadow -Werror`.
- Strict serial build of the six editor plus eight adjacent profile/transaction
  targets: clean exact-commit incremental build, exit `0`.
- Focused/adjacent CTest selector
  `^qindaqt\\.(customize-editor-|profile-|shell-customization-)`: **14/14
  passed**, exit `0`.
- Direct production dirty/hostile suite: **6/6 QtTest cases passed**, including
  both exact foreign-lease and foreign-preview rows, exit `0`.
- `./tools/check-source-shape`: **1,032 source files passed**, exit `0`;
  dirty-state test 129 nonblank lines and largest changed production source
  314, both below the decomposition-review boundary.
- `./tools/validate-docs`: **65 Markdown documents plus navigation passed**,
  exit `0`.
- isolated MkDocs 1.6.1 `mkdocs build --strict`: exit `0`.
- `git diff HEAD^ HEAD --check`: exit `0`; exact ten-path diff above.
- Provenance: exact parent is `e53a350`; base is an ancestor. Current `main`
  is `c4982697858c083828bd406f1aa56c4e942bcc10`, also the merge base;
  current-main changed-path overlap is empty and read-only merge-tree has zero
  conflict markers.
- Final `git status --porcelain=v1`: empty; preserved worktree is clean.

## Bounded caveat

This remains the presentation-independent editor domain. It does not claim the
later QML canvas, Settings route/composition, provisional shell binding, live
shell behavior, or nested visual qualification. No host GUI/input, compositor,
session bus, user configuration, shared roadmap/metrics, or integration branch
was touched.

— Nadia Park, exact repair handed off; not live.
