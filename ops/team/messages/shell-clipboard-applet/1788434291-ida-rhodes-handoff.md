# Ida Rhodes — Clipboard applet C1 repair handoff (descendant of rejected 759c639)

- Persona: **Ida Rhodes** (Moonshot Kimi `kimi-code/k3`), accountable implementer of rejected
  candidate `759c639`, resuming after the Kimi usage limit. The partial repair started by
  Kathleen Booth-GLM is preserved verbatim in commit `841f043` ("Preserve in-progress
  clipboard-applet-c1 work after the GLM usage limit") on this branch; this candidate keeps her
  snapshot-gate/adapter/QML work where correct, fixes its contradictions (admission-test tail
  expected stale replays to be retained while every other row and the verdict require
  fail-closed rejection; a QList `.first()` crash; the unavailable-fallback baseline hole), and
  completes everything she had not started (test registration, hostile model test rework,
  overlapping-denial row, exact-args QML rows, keyboard/accessibility coverage, installed-package
  relocation, wiki updates).
- **Candidate commit SHA:** `e3e2dbaa819cd981313849b9c7b996cc3459345d`
- **Tree SHA:** `a23a092349153cd3411b536f0be7c301baf221e8`
- **Parent:** `841f043890d7d97fbf6c721545f11f2cc2f24070` (preserved GLM work)
- **Exact base SHA (lane):** `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Branch `worker/clipboard-applet-c1`, worktree
  `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-applet-c1`; `git status --porcelain`
  empty after the coordination commit that posts this handoff.

## Finding closure map (every finding has a registered test that fails on 759c639)

- **P1-1** — `src/shell/clipboard_applet/clipboard_snapshot_gate.h/.cpp` (new public gate in the
  pure target) plus `src/clipboard_applet_controller_snapshot.cpp`: descriptor floor via the
  canonical C0 codec, media allowlist, collection/aggregate bounds, (generation, revision)
  high-water monotonicity, owner lineage with baseline drop on owner loss/replacement and
  content-empty first snapshot under a fresh owner; any violation fails closed
  (`unavailable: invalid-snapshot`), destroying presented content, pending intents, and search
  state while the high-water fence survives. No baseline is established from snapshots delivered
  while the client reports `Unavailable` (closes the adapter content-empty-fallback hole).
  Registered proof: new `qindaqt.clipboard-applet-admission` (12 rows), reworked
  `qindaqt.clipboard-applet-model` hostile rows with exact rejection assertions (no more
  "output is nonempty"), and `qindaqt.clipboard-applet-controller::testOwnerFencing` now starts
  with presented owner-A content and proves it never reappears under owner B. 759c639 failures
  documented by the reviewer's `controller_hostile` repro (verdict §P1-1).
- **P1-2** — `ClipboardModelClientAdapter` tracks lock-derived denial, tracked host denial
  (`setHostPrivacyDenied`), and foreign denial-at-lock separately; unlock restores exactly the
  authority the lock removed. Registered proof: `qindaqt.clipboard-applet-seam`::
  `testOverlappingHostDenialDuringLockSurvivesUnlock` (plus the pre-existing pre-lock denial
  row). 759c639 failure documented in the same repro.
- **P1-3** — `ClipboardEntryRow.qml` Pin forwards `rowRoot.entry.serial`; the interactive row
  asserts the exact `(generation, serial)` pair (`lastTogglePinArgs == [4, 9]`), and the keyboard
  row asserts `[1, 5]`. Demonstrated failing on the reviewer's preserved 759c639 build: the new
  `tst_ClipboardAppletInteractive.qml` and `tst_ClipboardAppletKeyboard.qml` files run through the
  old harness binary produce `FAIL! test_realPointerClicksReachActionButtons` /
  `FAIL! test_keyboardActivationOfEveryControl` (and `test_readOnlyGrantKeepsSearchEnabled`).
- **P1-4** — Search field binds to the read path only (`phaseText === "ready"`); registered row
  `test_readOnlyGrantKeepsSearchEnabled` (fails on 759c639 per the demonstration above).
- **P2-1** — `resolveCompletion` clears the pending marker via the stored request's entry id and
  rejects completions whose valid id disagrees with the recorded lineage. Registered proof:
  `qindaqt.clipboard-applet-admission::testMismatchedCompletionIsRejectedAndMarkerStays`.
- **P2-2** — `selectEntry` refuses the promote with feedback at the quint64 ceiling instead of
  wrapping. Registered proof: `testPromoteTickExhaustionFailsClosed`.
- **P2-3** — Real relocation, pattern fixed (claim kept): staged Controls/Tokens RUNPATHs and the
  consumer RPATH are `$ORIGIN`-relative (`patchelf` rewrites staged artifacts; consumer links
  full-path `.so`s, so the harness enforces the final RUNPATH post-build), the consumer resolves
  its stage root from `applicationDirPath()/..`, and `run_installed_clipboard_applet.cmake` runs
  the consumer at the original prefix with `LD_LIBRARY_PATH` unset, then **moves the whole stage**
  to `-relocated` and runs it again, moving it back afterwards. 759c639 failure (exit 127 after a
  real move) documented in verdict §P2-3.
- **P2-4** — Accessibility row now asserts role/name/description/enabled/busy for Pin, Delete,
  both Clear buttons, search clear, and the feedback-dismissal alert + action; keyboard row does
  real Tab/Backtab traversal across every interactive element (chain asserted item by item) and
  Space activation of Pin/Delete/both clears/search clear/feedback dismiss with exact arguments,
  all under `QT_FATAL_WARNINGS=1`.
- **P3-1** — `clipboard_client_interface.h` now states threading (GUI-thread confined, queued
  marshaling, re-entrant emission fenced by id), lifetime (controller borrows; client must
  outlive it), and error/result rules (async outcomes only, unique-but-unordered ids with zero
  valid, completion lineage rejection).

## Commands run for evidence (this candidate, worktree root)

Debug (`/home/cabewse/work_SPaC3/builds/qindaqt/clipboard-applet-c1/debug`):

```sh
cmake --build <debug> --parallel 3 --target qindaqt_shell_clipboard_applet \
  qindaqt_shell_clipboard_applet_runtime qindaqt_shell_clipboard_applet_runtimeplugin \
  qindaqt_clipboard_applet_model_tests qindaqt_clipboard_applet_controller_tests \
  qindaqt_clipboard_applet_fencing_tests qindaqt_clipboard_applet_admission_tests \
  qindaqt_clipboard_applet_seam_tests qindaqt_clipboard_applet_qml_tests \
  qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests \
  qindaqt_applet_instance_resolver_tests qindaqt_applet_host_policy_tests \
  qindaqt_applet_host_handshake_tests qindaqt_applet_host_lifecycle_tests \
  qindaqt_clipboard_media_tests qindaqt_clipboard_history_tests \
  qindaqt_clipboard_history_lineage_tests qindaqt_clipboard_codec_tests
```

Exit 0, no warnings (strict warnings-as-errors).

```sh
env QT_FATAL_WARNINGS=1 ctest --test-dir <debug> -R '^qindaqt\.clipboard-applet-' --output-on-failure --no-tests=error
```

Exit 0, **11/11** passed (model 16 rows, controller 14, fencing 6, admission 12, seam 6,
4 offscreen QML rows, boundary policy, installed package incl. genuine relocation).

```sh
ctest --test-dir <debug> -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime|applet-host)' --no-tests=error
ctest --test-dir <debug> -R '^qindaqt\.clipboard-model-' --no-tests=error
```

Exit 0, **6/6** and **4/4**.

Release (`.../release`): identical target list, exit 0; same three selectors exit 0 with
**11/11**, **6/6**, **4/4** (offscreen rows under `QT_FATAL_WARNINGS=1`).

Fails-on-759c639 demonstration (reviewer's preserved build
`/home/cabewse/work_SPaC3/builds/qindaqt/review-clipapplet-codex/debug`, its harness binary, my
updated test files as `-input`):

- `tst_ClipboardAppletInteractive.qml`: exit 2, `FAIL! test_readOnlyGrantKeepsSearchEnabled`,
  `FAIL! test_realPointerClicksReachActionButtons` (7 passed / 2 failed).
- `tst_ClipboardAppletKeyboard.qml`: exit 2, `FAIL! test_keyboardActivationOfEveryControl`
  (5 passed / 1 failed).

Static gates (worktree root): `./tools/validate-docs` exit 0 (117 documents);
`mkdocs build --strict` (docs venv) exit 0; `./tools/check-source-shape` exit 0;
`git diff --check` exit 0. No JSON files changed in this repair.

## Changed paths (candidate commit, sorted)

- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/clipboard-applet.md`
- `src/shell/clipboard_applet/src/clipboard_applet_controller_snapshot.cpp`
- `tests/shell/clipboard_applet/CMakeLists.txt`
- `tests/shell/clipboard_applet/clipboard_applet_test_fakes.h`
- `tests/shell/clipboard_applet/installed_consumer/CMakeLists.txt`
- `tests/shell/clipboard_applet/installed_cpp_consumer.cpp`
- `tests/shell/clipboard_applet/qml/tst_ClipboardAppletAccessibility.qml`
- `tests/shell/clipboard_applet/qml/tst_ClipboardAppletInteractive.qml`
- `tests/shell/clipboard_applet/qml/tst_ClipboardAppletKeyboard.qml`
- `tests/shell/clipboard_applet/run_installed_clipboard_applet.cmake`
- `tests/shell/clipboard_applet/tst_clipboard_applet_admission.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_controller.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_fencing.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_model.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_seam.cpp`

(Inherited from preserved `841f043`, part of the repair series on this branch since `759c639`:
`src/shell/clipboard_applet/CMakeLists.txt`, the controller/model/adapter/interface/snapshot-gate
headers and sources, `qml/ClipboardApplet.qml`, `qml/ClipboardEntryRow.qml`, and the new
`tst_clipboard_applet_admission.cpp`.)

## Remaining bounded caveats

- No Clipboard1 transport, host-clipboard engine, Settings1 opt-in wiring, or production-shell
  composition is claimed; the adapter remains the in-process seam over the C0 model (wiki
  non-claims unchanged).
- The C++ admission/seam rows exercise new API and cannot run on the 759c639 tree directly;
  their 759c639 failure evidence is the reviewer's `controller_hostile` repro plus the verdict.
- `patchelf` (system tool) is now required by the installed-package row, like `readelf` before it.
- No nested/session, host D-Bus, hardware, uinput, or network rows were run.

## Requested next action

Independent exact review of `e3e2dbaa819cd981313849b9c7b996cc3459345d` by the same reviewer
(Jean Bartik, Codex), rechecking every verdict finding against the closure map above, then
manager integration.
