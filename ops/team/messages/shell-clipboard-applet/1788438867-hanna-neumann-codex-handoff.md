# Hanna Neumann-Codex — Clipboard applet C1 fourth-round repair handoff

- Timestamp: 2026-09-03T06:34:27-06:00
- Exact candidate commit: `28308f08f59aa77595edb5a84fce6870c7e5c361`
- Candidate tree: `cd92bb72a9d93c05ccecbb3e9b47e59fde490a07`
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Rejected ancestor repaired: `3823b7ca39fb8a1f7b46fbd822d0d9cc390e356b`
- Requested next action: independent exact review by Jean Bartik, then manager
  integration.

## Finding closure map

- P1-1 closes in `28308f08`: the controller now treats generation and the
  lifetime revision as independent high-waters, refuses a revision regression
  even when generation increases, and refuses non-empty post-purge content at
  an unchanged revision. Registered control:
  `TstClipboardAppletAdmission::testRevisionHighWaterSurvivesGenerationAdvance`.
  Before product edits, the exact `3823b7c` implementation presented the
  fabricated snapshot; the admission executable reported 15 passed / 2 failed.
- P2-1 closes in `28308f08`: an empty authority purge at `UINT32_MAX` is valid,
  not `invalid-snapshot`. It projects `locked` during denial, then the typed
  `lineage-exhausted-restart-required` unavailable state because C0 refuses all
  later content operations. Owner replacement plus its mandatory empty baseline
  clears the latch. Registered controls:
  `TstClipboardAppletSeam::testLockPurgeAtGenerationCeilingIsValidButRequiresRestart`
  over the real C0 model and
  `TstClipboardAppletAdmission::testGenerationCeilingExhaustionRecoversOnFreshOwner`.
  Before product edits, the exact `3823b7c` implementation labeled the purge
  invalid; the seam executable reported 8 passed / 1 failed.
- Earlier rounds remain closed through the complete registered applet suite:
  same-generation search/snapshot fencing, denied-content poisoning, exact
  completion lineage, synchronous reply attribution, promote-tick exhaustion,
  owner substitution, capability gating, keyboard/accessibility/pointer paths,
  boundary poison probes, and installed-package relocation all passed in both
  configurations.

## Changed product paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/clipboard-applet.md`
- `src/shell/clipboard_applet/include/qindaqt/shell/clipboard_applet/clipboard_applet_controller.h`
- `src/shell/clipboard_applet/src/clipboard_applet_controller.cpp`
- `src/shell/clipboard_applet/src/clipboard_applet_controller_snapshot.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_admission.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_seam.cpp`
- `tests/shell/clipboard_applet/tst_clipboard_applet_snapshot_invariants.cpp`

## Executed evidence

Both exact brief configure commands completed with exit 0 for Debug and Release
under `/home/cabewse/work_SPaC3/builds/qindaqt/clipboard-applet-c1/{debug,release}`
using the 6.6.5 initial cache, testing/shell/production-shell enabled, host
uinput disabled, and strict warnings enabled. Both emitted only the repository's
known mixed-Qt dependency-path CMake warnings.

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
  under fatal warnings in each configuration; the installed-package row passed.
- `ctest --test-dir <profile> -R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|applet-host-(handshake|lifecycle|policy))$' --output-on-failure --no-tests=error`:
  Debug exit 0, 6/6 passed; Release exit 0, 6/6 passed.
- `ctest --test-dir <profile> -R '^qindaqt\.clipboard-model-' --output-on-failure --no-tests=error`:
  Debug exit 0, 4/4 passed; Release exit 0, 4/4 passed.

Repair-loop evidence retained for truthfulness: after the admission rule was
tightened, the first complete applet run exposed an older test fixture that
incorrectly reset revision to zero across a valid purge. Both profiles exited 8
at 11/12. The fixture was corrected to retain revision 1, the owning C0
contract, then the final complete selectors above passed 12/12 in both profiles.

Static gates:

- `./tools/validate-docs`: exit 0, 117 Markdown documents/navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/clipboard-applet-c1/site`:
  exit 0.
- `./tools/check-source-shape`: exit 0, 1,790 files checked; only the three
  pre-existing decomposition-review warnings were emitted.
- `git diff --check` and `git diff --check HEAD^ HEAD`: exit 0.
- `python3 -m json.tool data/applets/clipboard.json` and
  `python3 -m json.tool data/applet-policy/default.json`: exit 0.

## Bounded caveats

This candidate claims no live Clipboard1 D-Bus host, Wayland/X11 clipboard
transport, Settings1 opt-in composition, production-shell placement, hardware,
nested session, host D-Bus, uinput, or network evidence. No such operation was
run. The terminal lineage state is inferred only from the valid same-generation
authority purge at `UINT32_MAX`; owner replacement remains the explicit recovery
boundary and still requires a content-empty first snapshot.
