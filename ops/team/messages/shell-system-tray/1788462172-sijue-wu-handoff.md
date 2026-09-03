# Tray S2 handoff — compiled QST status-notifier applet over the accepted S1 transports

- Worker: Sijue Wu (Moonshot Kimi `kimi-code/k3`, reasoning high), slug `sijue-wu`
- Exact base: `893805724933b307e165fdefc485df1ae4a13015`
- Exact product candidate: `b10692c98f202e5bfc616fd9bfa0767f5bcda57e`
- Product candidate tree: `d8bb4a6b3aec5e406289728296e609ad64cda06b`
- Product candidate parent: `cfbe0cc61288bc5c157c0e55f9d66779defd8442` (claim-only coordination commit)
- Branch/worktree: `worker/tray-applet` at `/home/cabewse/work_SPaC3/container-wm-workers/tray-applet`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/tray-applet` (system-KWin initial cache per the lane override)

## Outcome delivered

1. `src/shell/status_notifier/applet/`: pure target `QindaQt::ShellStatusNotifierApplet`
   (`StatusNotifierAppletModel` projection over S1 `TrayPresentation` + descriptors; bounded
   24-row presentation with truthful overflow; depth-capped read-only menu preview) and runtime
   target `QindaQt::ShellStatusNotifierAppletRuntime` (`StatusNotifierAppletController` facade with
   immutable fail-closed `status-items.read`/`status-items.activate` gates, generation fencing,
   exactly-once dispatch with a reentrancy guard, GUI-thread PNG data-URL icon rendering with
   placeholder truth; `StatusNotifierMonitorAdapter` composing registry + S1 monitor + icon
   renderer behind a forwarding `StatusNotifierEventSink` shim for change notification; compiled
   `QindaQt.Shell.StatusNotifier 1.0` module with horizontal/vertical strips, keyboard traversal,
   accessible names/roles/states).
2. Registration: `data/applets/status-notifier.json`, two appended policy grants, one appended
   `firstParty()` entry (`qindaqt.applets.status-notifier`), additive integrity rows, the
   `StatusNotifierAppletRuntime` shell-carrying install component with closure-inventory
   membership, the installed-package relocation row, the boundary-poison gate, the
   `tests/shell/qml/imports/QindaQt/Shell/StatusNotifier/` dispatcher stub (access/theme/vertical),
   and DesktopVirtual staging in `tests/session/PanelVisibilityTests.cmake` (mirroring the
   historical Global Menu block, with an AGENT-NOTE; the module is not in
   `DesktopVirtualAppletModules.cmake` because that inventory is contractually limited to modules
   imported by `BuiltinAppletContent.qml` — hosting is a later lane).
3. Docs: `docs/wiki/shell/status-tray.md` (S2 sections), `applet-runtime.md` (registered,
   not yet hosted; nine registry entries, seven hosted), `module-boundaries.md` row,
   `testing-harness.md` rows, one additive catalog sentence in `applet-manifest-schema-v1.md`.

## Sorted changed paths (product commit `b10692c`)

- `data/applet-policy/default.json`
- `data/applets/status-notifier.json`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/applet-manifest-schema-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/status-tray.md`
- `src/CMakeLists.txt`
- `src/applet_runtime/src/builtin_applet_registry.cpp`
- `src/shell/CMakeLists.txt`
- `src/shell/status_notifier/applet/CMakeLists.txt`
- `src/shell/status_notifier/applet/include/qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h`
- `src/shell/status_notifier/applet/include/qindaqt/shell/status_notifier/applet/status_notifier_applet_model.h`
- `src/shell/status_notifier/applet/include/qindaqt/shell/status_notifier/applet/status_notifier_applet_types.h`
- `src/shell/status_notifier/applet/include/qindaqt/shell/status_notifier/applet/status_notifier_monitor_adapter.h`
- `src/shell/status_notifier/applet/include/qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h`
- `src/shell/status_notifier/applet/qml/StatusNotifierApplet.qml`
- `src/shell/status_notifier/applet/qml/StatusNotifierItemDelegate.qml`
- `src/shell/status_notifier/applet/src/status_notifier_applet_controller.cpp`
- `src/shell/status_notifier/applet/src/status_notifier_applet_model.cpp`
- `src/shell/status_notifier/applet/src/status_notifier_applet_types.cpp`
- `src/shell/status_notifier/applet/src/status_notifier_monitor_adapter.cpp`
- `tests/applet_runtime/tst_applet_instance_resolver.cpp`
- `tests/applets/tst_catalog.cpp`
- `tests/applets/tst_manifest.cpp`
- `tests/session/PanelVisibilityTests.cmake`
- `tests/shell/audio_applet/run_shell_component_closure.cmake`
- `tests/shell/qml/imports/QindaQt/Shell/StatusNotifier/StatusNotifierApplet.qml`
- `tests/shell/qml/imports/QindaQt/Shell/StatusNotifier/qmldir`
- `tests/shell/status_notifier/CMakeLists.txt`
- `tests/shell/status_notifier/applet/CMakeLists.txt`
- `tests/shell/status_notifier/applet/check_status_notifier_applet_boundary.cmake`
- `tests/shell/status_notifier/applet/installed_consumer/CMakeLists.txt`
- `tests/shell/status_notifier/applet/installed_cpp_consumer.cpp`
- `tests/shell/status_notifier/applet/qml_interactive_main.cpp`
- `tests/shell/status_notifier/applet/qml/tst_StatusNotifierApplet.qml`
- `tests/shell/status_notifier/applet/qml/tst_StatusNotifierAppletAccessibility.qml`
- `tests/shell/status_notifier/applet/qml/tst_StatusNotifierAppletKeyboard.qml`
- `tests/shell/status_notifier/applet/run_installed_status_notifier_applet.cmake`
- `tests/shell/status_notifier/applet/status_notifier_applet_test_fakes.h`
- `tests/shell/status_notifier/applet/tst_status_notifier_applet_adapter.cpp`
- `tests/shell/status_notifier/applet/tst_status_notifier_applet_controller.cpp`
- `tests/shell/status_notifier/applet/tst_status_notifier_applet_model.cpp`

Coordination-only paths (separate commits): `ops/team/messages/shell-system-tray/1788454890-sijue-wu-claim.md`,
this handoff, `ops/team/workers/sijue-wu.md`.

## Verification evidence (every command below was run by me on this candidate; exit 0 unless stated)

Configure (both profiles), lane-prescribed system-KWin cache:

```sh
cmake -S . -B <ROOT>/<debug|release> -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=<Debug|Release> -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Focused builds (`cmake --build <ROOT>/<profile> --parallel 3 --target ...`): Debug built the two
applet targets plus all status-notifier test executables, the applet integrity test executables,
`qindaqt-shell`, `qindaqt-shell-preview`, the Controls/Tokens/Launcher/GlobalMenu QML plugins, and
the desktop session probe targets; Release built the same set plus the S1 transport test
executables, `qindaqt_controls_font_pinning_tests`, and the three applet-host test executables.
All exit 0 under strict warnings.

Test rows, both profiles, under
`env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent` with host display
variables unset (`QT_QPA_PLATFORM=offscreen`, `QT_FATAL_WARNINGS=1` on the tray selector):

- `ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.status-notifier-' --output-on-failure --no-tests=error`
  — Debug 15/15 pass, Release 15/15 pass (7 pre-existing S1 rows + model, controller, adapter,
  qml-offscreen, qml-keyboard-offscreen, qml-accessibility-offscreen, boundary-policy,
  installed-package). QML rows ran after `qindaqt.controls-font-pinning` (pre-existing pinned-font
  fixture dependency, also 1/1 in both profiles).
- Applet integrity: `-R '^qindaqt\.(applet-manifest|applet-catalog|applet-runtime-resolution|applet-host-policy|applet-host-handshake|applet-host-lifecycle)$'`
  — Debug 6/6, Release 6/6.
- `-R '^qindaqt\.shell-runtime-component-closure$'` — Debug 1/1, Release 1/1 (all seven
  shell-carrying components incl. `StatusNotifierAppletRuntime`).
- `-R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)'` — Debug 3/3, Release 3/3.
  No nested rows were run.

Static gates (worktree root): `./tools/validate-docs` exit 0;
`/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`
exit 0; `./tools/check-source-shape` exit 0 (only the two pre-existing unrelated warnings);
`git diff --check` exit 0; `python3 -m json.tool` on both changed JSON files exit 0.
The boundary gate also passes standalone via `cmake -P
tests/shell/status_notifier/applet/check_status_notifier_applet_boundary.cmake`.

## Bounded caveats (deliberately not claimed)

- No panel hosting or production-shell dispatcher composition; the stock profile does not place
  the applet. `BuiltinAppletContent.qml`, `src/shell/runtime`, and panel QML are untouched.
- The older `system-tray` manifest (`status-tray.json`) remains an accepted contract resolving
  `implementation-unavailable`; reconciling it with the new `status-notifier` manifest belongs to
  the hosting lane, as does moving the DesktopVirtual staging into
  `DesktopVirtualAppletModules.cmake`.
- The descriptor menu preview is read-only; dbusmenu entry activation remains the Global Menu
  composition lane. Secondary activation stays truthfully pointer-only per the S1 texts.
- Evidence is source/unit/private-bus/offscreen level with fake items: no host session bus, no
  nested compositor, no assistive-technology bridge, no hardware/network claims.
- The QML rows depend on the pre-existing pinned-font artifact produced by
  `qindaqt.controls-font-pinning` (repository-wide fixture ordering, unchanged by this lane).

Requested next action: independent exact review then manager integration.
