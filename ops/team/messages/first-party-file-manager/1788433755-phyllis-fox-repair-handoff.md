# Phyllis Fox — File Manager S1 repair handoff

- Timestamp: 2026-09-03T05:09:15-06:00
- Candidate commit: `9ade95a9eea345ba7a19db9ef97d8bf126458817`
- Candidate tree: `e2554046e57952e338e71563c24b98e956432de2`
- Rejected candidate: `61283bf017990694a9ddc3f183f54751c3ddf849`
- Exact base: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Branch: `worker/file-manager-s1`
- Requested next action: independent exact review by the same reviewer, then
  manager integration.

## Finding closure

| Finding | Closing commit | Registered regression / evidence |
| --- | --- | --- |
| P1-1 | `9ade95a9eea345ba7a19db9ef97d8bf126458817` | `qindaqt.file-manager-mutation-ui-actions-offscreen` drives the real AppShell action, production `Main.qml`/`MutationDialogs.qml`, controller, and local backend for rename, copy, move, Trash, and restore. `runtime/mutation_ui_action_probe.cpp` names P1-1 in its `AGENT-NOTE:`. Listing identities now cross QML as exact decimal strings. |
| P2-1 | `9ade95a9eea345ba7a19db9ef97d8bf126458817` | `TestHomeTrash::orphanPayloadGetsSkippedByUniqueAllocator` names P2-1 and proves `files/item` without metadata advances to `item.1`. |
| P2-2 | `9ade95a9eea345ba7a19db9ef97d8bf126458817` | `qindaqt.file-manager-no-replace-race` names P2-2. Its preloaded racing writer creates `dest` after `fstatat` reports absence; `renameat2(RENAME_NOREPLACE)` returns typed `already-exists`, retaining source and attacker bytes. |
| P3-1 | `9ade95a9eea345ba7a19db9ef97d8bf126458817` | `TestMutationController::unchangedRenameIsNoOp` names P3-1 and proves no backend dispatch, busy state, or failure card state. |
| P3-2 | `9ade95a9eea345ba7a19db9ef97d8bf126458817` | `TestHomeTrash::vanishedRestoreParentIsTypedAsVanished` names P3-2 and distinguishes an unresolved parent from two confirmed unequal devices. |

The P1 action row also opened every identity-carrying production dialog under
fatal warnings and exposed a Fusion `Dialog.implicitWidth` loop in the Trash
confirmation. Explicit bounded widths close that warning there and in the
same-shaped Empty Trash confirmation without changing action semantics.

## Changed product paths

- `docs/wiki/adr/0064-confine-file-mutation-to-identity-checked-local-authority.md`
- `docs/wiki/apps/file-manager.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/file_manager/CMakeLists.txt`
- `src/apps/file_manager/main.cpp`
- `src/apps/file_manager/model/navigation_controller.cpp`
- `src/apps/file_manager/mutation/home_trash.cpp`
- `src/apps/file_manager/mutation/mutation_controller.cpp`
- `src/apps/file_manager/mutation/safe_path_operations.cpp`
- `src/apps/file_manager/runtime/mutation_ui_action_probe.cpp`
- `src/apps/file_manager/runtime/mutation_ui_action_probe.h`
- `src/apps/file_manager/ui/MutationDialogs.qml`
- `tests/apps/file_manager/CMakeLists.txt`
- `tests/apps/file_manager/race_writer_shim.cpp`
- `tests/apps/file_manager/tst_home_trash.cpp`
- `tests/apps/file_manager/tst_mutation_controller.cpp`
- `tests/apps/file_manager/tst_no_replace_race.cpp`

## Reproduction and negative-control evidence

- Reviewer's `qml_roundtrip` against `61283bf`, under offscreen Qt: exit 0 as
  a diagnostic; JavaScript changed `1788432896251134005` to
  `1788432896251134000`, UI rename finished `changed`, and the C++ control
  succeeded.
- Reviewer's `fm_attacks` against `61283bf`: exit 1 as expected; orphan-payload
  attack returned `already-exists` instead of allocating a suffix. Its a2 and
  a4 refusal controls passed; a3 produced the lexically resolved landing that
  the verdict explicitly classified as safe rather than a finding.
- Reviewer's `rename_race` plus its `LD_PRELOAD` writer against `61283bf`:
  diagnostic exit 0; mutation reported `none`, removed the source, and replaced
  `attacker-content` with `user-data`.
- A scratch P3 reproduction linked to the reviewer's exact old support library:
  exit 0 as a diagnostic; unchanged rename printed `already-exists`, and the
  unresolved restore parent printed `cross-device`.
- Current registered `tst_home_trash.cpp`, `tst_mutation_controller.cpp`, and
  `tst_no_replace_race.cpp` were separately compiled against the exact
  `61283bf` support library. The intentional negative run exited 1: the two
  new Home Trash cases failed 2/2, unchanged rename failed 1/1, and the
  no-replace race failed 1/1 after observing success rather than
  `already-exists`.

## Candidate verification

- Debug configure using the lane's exact cache/options recipe: exit 0.
- Release configure using the same recipe with `CMAKE_BUILD_TYPE=Release`:
  exit 0.
- Debug focused build of `qindaqt-file-manager`, all eight existing File Manager
  test executables, `qindaqt_file_manager_no_replace_race_tests`, and
  `qindaqt_file_manager_race_writer`, parallel 3: exit 0.
- Release build of the same focused targets, parallel 3: exit 0 (65 reported
  incremental build steps).
- Debug isolated selector:
  `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent HOME=<ROOT>/test-scratch/home
  XDG_DATA_HOME=<ROOT>/test-scratch/home/.local/share
  TMPDIR=<ROOT>/test-scratch/tmp QT_QPA_PLATFORM=offscreen
  QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/debug
  -R '^qindaqt\.file-manager-' --output-on-failure --no-tests=error`
  → exit 0, **16/16 passed**.
- Release identical isolated selector with `<ROOT>/release`: exit 0,
  **16/16 passed**.
- `./tools/validate-docs`: exit 0, **128 Markdown documents plus navigation**.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir <ROOT>/site`: exit 0.
- `./tools/check-source-shape`: exit 0, **2,069 files checked**; only the three
  pre-existing out-of-lane review-threshold warnings were reported.
- `git diff --check`: exit 0.
- `git show --check --oneline 9ade95a9eea345ba7a19db9ef97d8bf126458817`:
  exit 0.
- No JSON changed, so `python3 -m json.tool` was not applicable.

## Bounded caveats

- The atomic no-replace path is the Linux `renameat2` syscall. A kernel without
  that primitive fails closed as typed `unsupported`; this candidate does not
  claim a portable fallback for non-Linux platforms.
- This remains home Trash only. Per-volume Trash, mounts, search, previews,
  portals, network locations, nested screenshots, and whole-application AT
  qualification remain out of scope.
- No session/nested-compositor rows, host bus, hardware, uinput, or network
  calls were run.
