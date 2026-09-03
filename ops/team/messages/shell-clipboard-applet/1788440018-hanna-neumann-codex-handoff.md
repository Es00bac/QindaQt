# Hanna Neumann-Codex — Clipboard applet C1 fifth-round repair handoff

- Timestamp: 2026-09-03T06:53:38-06:00
- Exact candidate commit: `72a79fde5890b685dd09a872bc6e1350c5ebe14d`
- Candidate tree: `1b8b52eb1d47e65dba4cce05390b825e2bf55707`
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Rejected descendant repaired: `28308f08f59aa77595edb5a84fce6870c7e5c361`
- Requested next action: independent exact review by Jean Bartik, then manager
  integration.

## Finding closure map

- P2-1 closes in `72a79fde`: the controller keeps its valid generation-ceiling
  exhaustion latch subordinate to active session-lock, privacy, and history
  denial. Independent host privacy denial therefore presents the registered
  `locked` / privacy-policy reason with no rows; after authority returns, the
  same empty exhausted lineage presents typed
  `lineage-exhausted-restart-required` unavailability. Registered real-C0
  control:
  `TstClipboardAppletSeam::testHostPrivacyPurgeAtGenerationCeilingPreservesDeniedPhase`.
  Before the product edit, that new row failed on unchanged `28308f0` with 2
  passed / 1 failed process events (`unavailable` actual, `locked` expected).
  It passes 3/3 after the repair.
- The fake-client owner-recovery row now asserts the same denial-then-terminal
  ordering, while the prior session-lock ceiling row proves lock presentation
  and exhaustion remain intact. Each direct Debug selector passes 3/3 process
  events. The complete registered matrices keep every earlier-round closure
  green in both configurations.

## Changed product paths

- `docs/wiki/shell/clipboard-applet.md`
- `src/shell/clipboard_applet/include/qindaqt/shell/clipboard_applet/clipboard_applet_controller.h`
- `src/shell/clipboard_applet/src/clipboard_applet_controller.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_admission.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_seam.cpp`

## Executed evidence

The exact brief configure recipe completed with exit 0 for Debug and Release
under `/home/cabewse/work_SPaC3/builds/qindaqt/clipboard-applet-c1/{debug,release}`
using the private KWin 6.6.5 initial cache, testing/shell/production-shell
enabled, host uinput disabled, and strict warnings enabled. Both emitted only
the repository's known mixed-Qt dependency-path CMake warnings.

The following focused build completed with exit 0 in both Debug and Release:

```sh
cmake --build <profile> --parallel 3 --target \
  qindaqt_shell_clipboard_applet qindaqt_shell_clipboard_applet_runtime \
  qindaqt_shell_clipboard_applet_runtimeplugin \
  qindaqt_clipboard_applet_model_tests qindaqt_clipboard_applet_controller_tests \
  qindaqt_clipboard_applet_fencing_tests qindaqt_clipboard_applet_admission_tests \
  qindaqt_clipboard_applet_snapshot_invariant_tests \
  qindaqt_clipboard_applet_seam_tests qindaqt_clipboard_applet_qml_tests \
  qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests \
  qindaqt_applet_instance_resolver_tests qindaqt_applet_host_policy_tests \
  qindaqt_applet_host_handshake_tests qindaqt_applet_host_lifecycle_tests \
  qindaqt_clipboard_media_tests qindaqt_clipboard_history_tests \
  qindaqt_clipboard_history_lineage_tests qindaqt_clipboard_codec_tests
```

Final selectors all used
`env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`:

- `QT_FATAL_WARNINGS=1 ctest --test-dir <profile> -R '^qindaqt\.clipboard-applet-' --output-on-failure --no-tests=error`:
  Debug exit 0, 12/12 passed; Release exit 0, 12/12 passed. Four QML rows ran
  under fatal warnings in each profile; the installed-package row passed.
- `ctest --test-dir <profile> -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|applet-host-(handshake|lifecycle|policy))$' --output-on-failure --no-tests=error`:
  Debug exit 0, 6/6 passed; Release exit 0, 6/6 passed.
- `ctest --test-dir <profile> -R '^qindaqt\.clipboard-model-' --output-on-failure --no-tests=error`:
  Debug exit 0, 4/4 passed; Release exit 0, 4/4 passed.

Direct Debug repair selectors under the same poisoned D-Bus environment:

- `qindaqt_clipboard_applet_seam_tests testHostPrivacyPurgeAtGenerationCeilingPreservesDeniedPhase`:
  exit 0, 3/3 process events after the repair; before the product edit it exited
  1 with 2 passed / 1 failed.
- `qindaqt_clipboard_applet_admission_tests testGenerationCeilingExhaustionRecoversOnFreshOwner`:
  exit 0, 3/3 process events.
- `qindaqt_clipboard_applet_seam_tests testLockPurgeAtGenerationCeilingIsValidButRequiresRestart`:
  exit 0, 3/3 process events.

Static gates:

- `./tools/validate-docs`: exit 0, 117 Markdown documents/navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-applet-c1/site`:
  exit 0.
- `./tools/check-source-shape`: exit 0, 1,790 files checked; only the three
  pre-existing decomposition-review warnings were emitted.
- `git diff --check`: exit 0.
- No JSON changed, so no JSON parser gate applied.

## Bounded caveats

This candidate claims no live Clipboard1 D-Bus host, Wayland/X11 clipboard
transport, Settings1 opt-in composition, production-shell placement, hardware,
nested session, host D-Bus, uinput, or network evidence. No such operation was
run. The typed terminal state remains restricted to an accepted, empty
same-generation authority purge at `UINT32_MAX` after all authorities return;
owner replacement and its content-empty initial baseline remain the recovery
boundary.
