# Independent exact-candidate review — File Manager S1 repair recheck (R2)

- Reviewer: **Kathleen Antonelli**, independent application reviewer (Moonshot Kimi `kimi-code/k3`), slug `kathleen-antonelli`
- Candidate SHA: `9ade95a9eea345ba7a19db9ef97d8bf126458817` (branch `worker/file-manager-s1`)
- Tree SHA: `e2554046e57952e338e71563c24b98e956432de2`
- Parent SHA: `a5a6240bfcad1868261b8d733bfa6792893101a9`
- Base SHA: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Rejected product ancestor: `61283bf017990694a9ddc3f183f54751c3ddf849` (my prior verdict: REJECT 0/1/2/2)
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/file-manager-s1-k3-review` (detached at candidate; `git rev-parse HEAD` verified; `git status --porcelain` empty before and after)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-fm-k3` (incremental rebuilds of the ancestor configurations); scratch under `<ROOT>/scratch/`

## Findings ledger

### P0 — none

### P1 — none

### P2 — none

### P3 — none

All five prior findings are closed by code plus registered tests, each proven
to fail on the exact rejected ancestor (negative controls below). No new
defects found in the repair diff `61283bf..9ade95a`.

## Closure evidence per finding

### P1-1 (QML identity round trip) — closed

- Repair: identity fields cross QVariant→JS→QVariant as canonical decimal
  strings (`navigation_controller.cpp:166-173`, with an `AGENT-GUARD` naming
  P1-1); `MutationController::identityFromMap` strict-parses all five fields
  as base-10 strings with overflow checks and rejects any non-string or
  unparseable field (`mutation_controller.cpp:37-58, 236-249`).
- Registered proof: new row `qindaqt.file-manager-mutation-ui-actions-offscreen`
  runs the production binary with `--check-ui-actions`
  (`runtime/mutation_ui_action_probe.cpp`), driving real AppShell actions
  (`file.rename/copy/move/trash/restore-last`) through production
  `Main.qml`/`MutationDialogs.qml` into the local backend, asserting committed
  bytes and `canRestore()`. Passed in Debug and Release under
  `QT_FATAL_WARNINGS=1`.
- My scratch rerun (`scratch/qml-roundtrip/`, same driver source as the
  original reproduction, relinked against the descendant's
  `libqindaqt_file_manager_support.a`, offscreen Qt):

  ```
  true modifiedNanoseconds: 1788434193995474352
  js sees modifiedNanoseconds: 1788434193995474352|string
  renameItem accepted: true
  failureCode: none
  renamed exists: yes | original exists: no
  ```

  The exact value survives the JS round trip and the UI-path rename commits.
  (The driver's trailing C++ control prints `vanished` only because the
  UI-driven rename already consumed the source — harness sequencing, not
  product behavior.)
- Negative control: the same driver source built against the ancestor library
  (`scratch/qml-roundtrip-ancestor/`, linked to
  `scratch/ancestor-build/.../libqindaqt_file_manager_support.a` built from a
  `git archive` of `61283bf`) reproduces the original defect exactly:

  ```
  true modifiedNanoseconds: 1788434459877744154
  js sees modifiedNanoseconds: 1788434459877744000|number
  failureCode: changed (The selected item changed before the operation began)
  control C++ renameItem accepted: true | failureCode: none
  ```

  The registered UI-actions row cannot exist on the ancestor (no
  `--check-ui-actions`), but its assertion (`failureCode == none` after a
  QML-dispatched rename) is precisely what fails there, as shown above.

### P2-1 (orphan Trash payload wedge) — closed

- Repair: `HomeTrash::trash` advances the bounded suffix allocator when the
  payload relocate returns `AlreadyExists`, not only on metadata collision
  (`home_trash.cpp:226-231`, `AGENT-NOTE` naming P2-1).
- Scratch rerun (`scratch/attacks`, attack a1, descendant library): second
  trash of a same-named item with an orphan `files/item` now returns `none`
  and succeeds via suffix; source removed. Previously: permanent
  `already-exists`.
- Registered proof: `TestHomeTrash::orphanPayloadGetsSkippedByUniqueAllocator`
  asserts token `item.1` and both payloads retained.
- Negative control: the new `tst_home_trash.cpp` compiled and run against the
  exact ancestor library fails 2/2 new cases:

  ```
  FAIL!  : TestHomeTrash::orphanPayloadGetsSkippedByUniqueAllocator() 'second.ok()' returned FALSE. (The destination already exists)
  FAIL!  : TestHomeTrash::vanishedRestoreParentIsTypedAsVanished() Compared values are not the same
  ```

### P2-2 (racing-destination overwrite) — closed

- Repair: commit is `renameat2(RENAME_NOREPLACE)` via raw syscall
  (`safe_path_operations.cpp:153-170, 380-382`), with `ENOSYS`/`ENOTSUP`
  mapped to typed `unsupported` (fail closed on kernels lacking the
  primitive); the preflight `fstatat` remains diagnostic only; rollback is
  also no-replace (strictly stricter than the ancestor's clobbering
  `renameat` rollback).
- Scratch rerun: my old shim interposed `renameat`, which the repaired code
  no longer calls; I re-armed the race from `fstatat`
  (`scratch/rename-race/shim.c`, mirroring the registered
  `race_writer_shim.cpp`). Against the descendant:

  ```
  --- control (no shim) ---
  rename error: none; dest = user-data; victim gone        (normal path intact)
  --- attack (fstatat shim creates dest mid-window) ---
  rename error: already-exists
  dest contents after operation: attacker-content          (attacker preserved)
  victim exists: yes                                       (source retained)
  ```

- Registered proof: `qindaqt.file-manager-no-replace-race` with the preloaded
  `qindaqt_file_manager_race_writer` passed in both profiles.
- Negative control: the new `tst_no_replace_race.cpp` compiled against the
  ancestor library, run with the shim, fails as expected (ancestor observed
  success, i.e. silent overwrite):

  ```
  FAIL!  : TestNoReplaceRace::racingWriterCannotBeOverwritten() Compared values are not the same
  ```

### P3-1 (unchanged rename) — closed

`MutationController::renameItem` returns true as a no-op when `newName` equals
the current file name, before backend dispatch (`mutation_controller.cpp:119-124`).
Registered `TestMutationController::unchangedRenameIsNoOp` asserts no dispatch,
no busy state, `failureCode == none`; passes on the descendant, fails on the
ancestor (`'!controller.busy()' returned FALSE` — the ancestor dispatched).

### P3-2 (vanished restore parent typing) — closed

`HomeTrash::restore` now separates unresolved device lookup (`vanished`) from
two resolved, unequal devices (`cross-device`) (`home_trash.cpp:293-301`).
Registered `TestHomeTrash::vanishedRestoreParentIsTypedAsVanished` passes on
the descendant, fails on the ancestor (negative control above).

## Regression re-probes on the descendant (review question 2)

- Containment: symlinked intermediate parent rename still refused
  `symlink-escape`, target untouched (attack a2); re-trash of an item inside
  Trash still `invalid-request` (a4); Empty Trash symlink-no-follow covered by
  the passing registered test. Attack a3 (`sub/../../escaped`) behaves exactly
  as classified in my prior verdict: the lexical clean lands at the declared
  root-contained destination `fixture/escaped` with identity checks intact —
  the a3 "FAIL" lines in my scratch output are my own stale pre-verdict
  expectations, not product defects.
- Cancellation / half-written targets: the repair does not touch the copy or
  cancellation paths (only the shared `errorForErrno` gained fail-closed
  `ENOSYS`/`ENOTSUP` mapping). Registered proofs passed in both profiles:
  `preCancelledCopyDoesNotCreateDestination`,
  `cancellationDuringCopyRemovesPartialDestination`,
  `cancellationCompletesWithTypedFailure`.
- Repair-diff scope audit (`git diff --stat 61283bf..9ade95a`): only owned
  File Manager product/test/wiki paths plus the implementer's own
  `ops/team` message and worker record. No shared-registry edits, no
  out-of-lane product changes. `MutationError::Unsupported` pre-existed in
  `mutation_types.h:35` (verified at `61283bf`). No other consumer of the
  entries-map identity keys exists outside the mutation dispatch path
  (grepped). The QML dialog width additions are bounded
  (`Math.min(560, root.width - ...)`) and pass under `QT_FATAL_WARNINGS=1`.
- Handoff's claims match my independent runs; the Linux-only caveat
  (`renameat2`, ENOSYS→`unsupported`) is documented in the ADR/wiki and is
  fail-closed, consistent with the handoff's bounded caveats.

## Commands executed

- `git rev-parse HEAD` → `9ade95a9...`; `git status --porcelain` empty before
  and after; tree/parent as in header.
- Debug incremental build: `cmake --build <ROOT>/debug --parallel 3 --target
  qindaqt-file-manager qindaqt_file_manager_history_tests
  qindaqt_file_manager_local_mutation_tests
  qindaqt_file_manager_home_trash_tests
  qindaqt_file_manager_mutation_controller_tests
  qindaqt_file_manager_action_catalog_tests
  qindaqt_file_manager_local_lister_tests
  qindaqt_file_manager_launch_intent_tests
  qindaqt_file_manager_controller_tests
  qindaqt_file_manager_no_replace_race_tests
  qindaqt_file_manager_race_writer` → exit 0 (46 steps).
- Release incremental build, same target set → exit 0 (46 steps).
- Debug selector: `env -u DISPLAY -u WAYLAND_DISPLAY -u
  DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent
  HOME=<ROOT>/scratch/home XDG_DATA_HOME=<ROOT>/scratch/home/.local/share
  TMPDIR=<ROOT>/scratch QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
  QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/debug -R
  '^qindaqt\.file-manager-' --output-on-failure --no-tests=error` → exit 0,
  **16/16 passed** (includes the new `-mutation-ui-actions-offscreen` and
  `-no-replace-race` rows).
- Release identical selector against `<ROOT>/release` → exit 0, **16/16
  passed**.
- `./tools/validate-docs` → exit 0 (128 Markdown documents plus navigation).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir <ROOT>/site` → exit 0.
- `./tools/check-source-shape` → exit 0 (2,069 files; only pre-existing
  out-of-lane review-threshold warnings, none in candidate paths).
- `git diff --check` → exit 0. No JSON changed in `61283bf..9ade95a`, so
  `json.tool` was not applicable.
- Ancestor negative control: `git archive 61283bf | tar -x` under
  `scratch/ancestor-src` (scratch copy, no repo mutation), configured with the
  lane cache recipe, built `qindaqt_file_manager_support`; new registered test
  sources compiled into that tree and run: `ctest -R
  'qindaqt\.file-manager-(home-trash|mutation-controller|no-replace-race)'` →
  exit 8, **0/3 suites passed, 4 new cases failed** (the exact failures quoted
  above). Pre-existing ancestor cases in those suites still passed, isolating
  the repair as the delta.
- Scratch reproductions (all under `<ROOT>/scratch/`, rebuilt against the
  descendant Debug library): `qml-roundtrip` (P1-1 closed; ancestor-linked
  variant reproduces P1-1), `attacks` (a1 closed; a2/a4 refusals intact),
  `rename-race` with re-armed `fstatat` shim (P2-2 closed, control intact).
- Never run: `tests/session` rows, host D-Bus system/session services,
  hardware, uinput, network.

## Verdict

Every finding from the `61283bf` rejection is closed by a registered test that
I independently proved fails on the exact rejected ancestor, and every scratch
reproduction from my prior verdict now shows the contracted behavior on the
descendant. Both profiles pass the full File Manager selector with isolation,
and all static gates pass. No regressions found in the repair diff.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
