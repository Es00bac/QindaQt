# Lauren Williams — independent shell review

- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate: `1e32f0a211cd6078652ca0d8d92750ec942b92c6`
- Tree: `4f391e41d09de7816c9e9637d56c0654d9cc1c3d`
- Parent/rejected candidate: `da9f2fdcc54cd8b0ac2db971736f9ab466f72ad7`
- Base of the rejected candidate: `8cd28a796f84d866a867093567efbed3c0f01a2f`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/task-list-applet-codex-review`
- Review build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-tlapplet-r2-codex`
- Review surface: exact repair diff `da9f2fdcc54cd8b0ac2db971736f9ab466f72ad7..1e32f0a211cd6078652ca0d8d92750ec942b92c6` (15 paths, 638 insertions, 164 deletions), prior verdict, repair handoff, owning wiki contracts, focused/adjacent runtime evidence, rejected-parent negative controls, and static gates.

## Findings ledger

### P0

None.

### P1

None.

Both prior P1 findings are closed:

1. The dock dispatch now records one token against both participants at `src/shell/task_list/applet/src/task_list_applet_controller_intents.cpp:141`; `PendingOperation` carries the participant list at `src/shell/task_list/applet/include/qindaqt/shell/task_list/applet/task_list_applet_controller.h:122-145`. The real controller/bridge/adapter/transport test at `tests/shell/task_list/tst_task_list_applet_operations.cpp:182` refuses activate, minimize, and reverse-dock attempts for the incoming participant until the single terminal result. Direct candidate execution passed in both profiles (3 passed, 0 failed). Copying that exact repaired test into a scratch export of rejected parent `da9f2fd` produced exit 1 at line 201: the parent returned true for `activateTask("c1", revision)` where false was expected (2 passed, 1 failed). This is a non-vacuous regression fence.
2. Both production QML files explicitly import `QindaQt.Controls 1.0` and `QindaQt.Tokens 1.0` (`TaskListApplet.qml:7-8`, `TaskListEntryButton.qml:7-8`), use first-party primitives/tokens, and carry no hard-coded six-digit palette literal or untyped theme map. The registered boundary poison row and offscreen QML rows passed. The relocated installed-package row passed in both profiles and instantiated the staged applet using staged Tokens/Controls artifacts. Running the candidate boundary probe against rejected parent `da9f2fd` exited 1 on `TaskListApplet.qml` with `task-list applet QML carries a hard-coded palette literal`, proving the control rejects the prior violation.

### P2

None.

The prior P2 is closed. `qindaqt.task-list-applet-qml-arrows-offscreen` is separately registered at `tests/shell/task_list/CMakeLists.txt:271`; its runtime slot at `tests/shell/task_list/tst_task_list_applet_qml.cpp:326` exercises horizontal Left/Right and vertical Up/Down traversal, endpoint stability, inert cross-axis keys, and zero dispatch. It passed through CTest and directly under `QT_FATAL_WARNINGS=1` in both profiles (3 passed, 0 failed each).

### P3

None.

The prior P3 is closed: the component-closure inventory now names `TaskListAppletRuntime` at `docs/wiki/development/testing-harness.md:197`, and the owning task-list page documents the participant fence, QST/Controls boundary, and focused evidence.

## Commands and results

All runtime commands below used the fail-closed environment `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`. No nested-compositor/session row, host D-Bus service, hardware, uinput, or network operation was run.

### Identity and cleanliness

```sh
git rev-parse HEAD
git rev-parse 'HEAD^{tree}'
git rev-parse HEAD^
git status --porcelain
```

Initial and final result: exit 0; SHA/tree/parent exactly matched the header and status output was empty. Final `git diff --check` also exited 0 with no output. No product path was edited.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tlapplet-r2-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-tlapplet-r2-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Result: exit 0 for both. The generated compositor metadata records KWin ABI `6.6.5`, as required for this immutable candidate. CMake emitted non-fatal mixed-prefix runtime-path warnings; generation completed.

### Builds

For each of `debug` and `release`:

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_task_list_values_tests qindaqt_task_list_source_grouping_tests \
  qindaqt_task_list_source_validation_tests qindaqt_task_list_intents_tests \
  qindaqt_task_list_scope_filter_tests qindaqt_task_list_presentation_tests \
  qindaqt_task_list_wire_tests qindaqt_task_list_facts_producer_tests \
  qindaqt_task_list_operation_adapter_tests qindaqt_task_list_operation_results_tests \
  qindaqt_task_list_operation_lineage_tests qindaqt_task_list_qt_transports_tests \
  qindaqt_task_list_applet_projection_tests qindaqt_task_list_applet_controller_tests \
  qindaqt_task_list_applet_operations_tests qindaqt_task_list_applet_qml_tests \
  qindaqt_applet_instance_resolver_tests qindaqt_applet_host_policy_tests \
  qindaqt_applet_host_handshake_tests qindaqt_applet_host_lifecycle_tests \
  qindaqt_shell_runtime_options_tests qindaqt-shell
```

Result: exit 0 in both profiles (766 Ninja steps each).

The first selector attempt accurately exposed omitted test prerequisites rather than a product failure: two applet test binaries were not built and component closure lacked `qindaqt-shell-preview`. After the following additive builds, a second attempt showed only a missing staged launcher plugin; the plugin build and the isolated closure rerun then passed.

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt-shell-preview
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt_shell_launcher_qmlplugin qindaqt_global_menu_qmlplugin \
  qindaqt_shell_audio_applet_runtimeplugin qindaqt_shell_bluetooth_applet_runtimeplugin \
  qindaqt_shell_power_applet_runtimeplugin qindaqt_controls_qmlplugin qindaqt_tokens_qmlplugin
```

Result: exit 0 in both profiles; isolated `qindaqt.shell-runtime-component-closure` passed 1/1 in each.

For the safe DesktopVirtual package-contract adjacency, each profile built the declared artifacts (755/755, exit 0):

```sh
cmake --build <ROOT>/<profile> --parallel 3 --target \
  qindaqt-wm qindaqt-session qindaqt-notification-host qindaqt-shell \
  qindaqt-settings-service qindaqt-audio-service qindaqt-settings qindaqt-editor \
  qindaqt_app_shell qindaqt_tokens_qml qindaqt_tokens_qmlplugin \
  qindaqt_controls_qml qindaqt_controls_qmlplugin \
  qindaqt_settings_appearance_qml qindaqt_settings_appearance_qmlplugin \
  qindaqt_settings_display_qml qindaqt_settings_display_qmlplugin \
  qindaqt_settings_network_qml qindaqt_settings_network_qmlplugin \
  qindaqt_settings_audio_qml qindaqt_settings_audio_qmlplugin \
  qindaqt_settings_bluetooth_qml qindaqt_settings_bluetooth_qmlplugin \
  qindaqt_settings_power_qml qindaqt_settings_power_qmlplugin \
  qindaqt_compositor qindaqt_decoration
```

### Safe selectors and direct runtime attacks

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <ROOT>/<profile> \
  -R '^qindaqt\.(task-list-|applet|shell-runtime-)' \
  --output-on-failure --no-tests=error
```

Final result: Debug 30/30 passed, exit 0; Release 30/30 passed, exit 0. This includes task-list rows 86-106, the boundary poison row, offscreen arrow row, relocated installed-package row, and shell runtime component closure.

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <ROOT>/<profile> \
  -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)' \
  --output-on-failure --no-tests=error
```

Result: Debug 2/2 passed, exit 0; Release 2/2 passed, exit 0. This tree contains the `sandbox-unit` and `package-contract` matches; no nested row was selected.

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  <ROOT>/<profile>/tests/shell/task_list/qindaqt_task_list_applet_operations_tests \
  dockDispatchFencesBothParticipantsUntilTerminalResult
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen \
  QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  <ROOT>/<profile>/tests/shell/task_list/qindaqt_task_list_applet_qml_tests \
  arrowTraversalStopsAtStripEndpoints
```

Result: both commands exited 0 in both profiles; each reported 3 passed, 0 failed.

### Rejected-parent negative controls

The scratch export and replacement test live only under the assigned build root:

```sh
mkdir -p <ROOT>/parent-da9f2fd-src
git archive da9f2fdcc54cd8b0ac2db971736f9ab466f72ad7 | tar -x -C <ROOT>/parent-da9f2fd-src
cp tests/shell/task_list/tst_task_list_applet_operations.cpp \
  <ROOT>/parent-da9f2fd-src/tests/shell/task_list/tst_task_list_applet_operations.cpp
cmake -S <ROOT>/parent-da9f2fd-src/tests/shell/task_list \
  -B <ROOT>/parent-da9f2fd-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build <ROOT>/parent-da9f2fd-debug --parallel 3 \
  --target qindaqt_task_list_applet_operations_tests
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  <ROOT>/parent-da9f2fd-debug/qindaqt_task_list_applet_operations_tests \
  dockDispatchFencesBothParticipantsUntilTerminalResult
```

Configure/build exited 0. The test exited 1 as expected: 2 passed, 1 failed; actual `activateTask("c1", revision)` was true versus expected false at copied-test line 201.

```sh
cmake -DSOURCE_ROOT=<ROOT>/parent-da9f2fd-src \
  -P tests/shell/task_list/check_task_list_applet_boundary.cmake
```

Result: exit 1 as expected, rejecting the parent `TaskListApplet.qml` hard-coded palette literal.

### Static/documentation gates

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir <ROOT>/site
./tools/check-source-shape
git diff --check da9f2fdcc54cd8b0ac2db971736f9ab466f72ad7..1e32f0a211cd6078652ca0d8d92750ec942b92c6
git diff --name-only da9f2fdcc54cd8b0ac2db971736f9ab466f72ad7..1e32f0a211cd6078652ca0d8d92750ec942b92c6 -- '*.json'
```

Results: all commands exited 0. Documentation validated 135 Markdown documents and strict MkDocs completed. Source shape checked 2346 files; it emitted only existing decomposition-review warnings outside this repair's changed implementation/test files. Diff check had no output. The changed-JSON query had no output, so there was no changed JSON file on which to run `python3 -m json.tool`.

## Verdict

The exact candidate closes every prior P1/P2/P3 finding with executable, non-vacuous evidence and introduces no new P0-P2 defect. The task-list fence, QST/Controls presentation and relocated component boundaries, registered directional traversal, documentation, and safe adjacent package contracts all pass in Debug and Release. ACCEPT.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
