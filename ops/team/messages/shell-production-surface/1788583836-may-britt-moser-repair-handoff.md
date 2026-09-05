# Handoff — Shell iconography I2 bounded repair

- Worker: May-Britt Moser (`may-britt-moser`), OpenAI Codex `gpt-5.6-sol`, reasoning high.
- Exact candidate commit: `7eb5372d8cc1a381dcf9fb4f1463032b1f018487`.
- Candidate tree: `63dc9d5c004b2fca94ea7f55037ded202e7ea6a6`.
- Repaired candidate: `0af5d685a36e8c5bb0e00fc708248e93b92ae739`.
- Exact base: `a5d78e804f28b5e11df8216ef8e0995f5d9d5191`.
- Requested next action: independent exact review then manager integration.

## Changed paths

- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/theme-schema-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/audio-applet.md`
- `docs/wiki/shell/iconography.md`
- `docs/wiki/shell/panel-surfaces.md`
- `docs/wiki/shell/status-tray.md`
- `src/shell/ShellAppletRuntimeInstall.cmake`
- `src/shell/app/shellpreviewapplication.cpp`
- `src/shell/app/shellpreviewapplication.h`
- `src/shell/audio_applet/qml/AudioApplet.qml`
- `src/shell/bluetooth_applet/qml/BluetoothApplet.qml`
- `src/shell/clipboard_applet/qml/ClipboardPanelApplet.qml`
- `src/shell/common/shelliconconfiguration.cpp`
- `src/shell/common/shelliconconfiguration.h`
- `src/shell/launcher/CMakeLists.txt`
- `src/shell/launcher/qml/LauncherApplet.qml`
- `src/shell/power_applet/qml/PowerApplet.qml`
- `src/shell/qml/AppletChip.qml`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `src/shell/runtime/shellruntimeapplication_applets.cpp`
- `src/shell/runtime/shellruntimeapplication_tokens.cpp`
- `src/shell/status_notifier/applet/qml/StatusNotifierApplet.qml`
- `src/shell/task_list/applet/qml/TaskListApplet.qml`
- `src/themes/include/qindaqt/themes/theme_spec.h`
- `src/themes/src/theme_loader.cpp`
- `src/themes/src/theme_spec.cpp`
- `tests/shell/CMakeLists.txt`
- `tests/shell/audio_applet/CMakeLists.txt`
- `tests/shell/audio_applet/run_shell_component_closure.cmake`
- `tests/shell/audio_applet/tst_audio_applet_qml.cpp`
- `tests/shell/audio_applet/verify_audio_summary_icon_contract.cmake`
- `tests/shell/bluetooth_applet/CMakeLists.txt`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_qml.cpp`
- `tests/shell/clipboard_applet/CMakeLists.txt`
- `tests/shell/clipboard_applet/qml/tst_ClipboardProductionPanelKeyboard.qml`
- `tests/shell/clipboard_applet/qml_interactive_main.cpp`
- `tests/shell/icon_resolution_test_fixture.h`
- `tests/shell/launcher/CMakeLists.txt`
- `tests/shell/launcher/tst_launcher_dispatcher.qml`
- `tests/shell/launcher/tst_launcher_qml.cpp`
- `tests/shell/power_applet/CMakeLists.txt`
- `tests/shell/power_applet/tst_power_applet_qml.cpp`
- `tests/shell/qml/imports/QindaQt/Shell/Icons/Icon.qml`
- `tests/shell/qml/imports/QindaQt/Shell/Launcher/LauncherApplet.qml`
- `tests/shell/qml/imports/QindaQt/Tokens/Tokens.qml`
- `tests/shell/status_notifier/applet/qml/tst_StatusNotifierApplet.qml`
- `tests/shell/status_notifier/applet/qml/tst_StatusNotifierAppletAccessibility.qml`
- `tests/shell/task_list/CMakeLists.txt`
- `tests/shell/task_list/task_list_applet_qml_theme_fixture.h`
- `tests/shell/task_list/tst_task_list_applet_qml.cpp`
- `tests/shell/testdata/breeze-icon-names.txt`
- `tests/shell/testdata/icon-themes-invalid/bad.json`
- `tests/shell/testdata/icon-themes-valid/default-light.json`
- `tests/shell/testdata/icon-themes/bad.json` (removed/moved)
- `tests/shell/testdata/icon-themes/default-light.json` (removed/moved)
- `tests/shell/tst_shelliconconfiguration.cpp`
- `tests/shell/verify_shell_icon_coverage.cmake`

## Acceptance evidence

- Exact Debug and Release configure recipe from the brief: exit 0 in both profiles.
- `cmake --build <ROOT>/{debug,release} --parallel 3 --target qindaqt-shell qindaqt-shell-preview qindaqt_audio_applet_qml_tests qindaqt_bluetooth_applet_qml_tests qindaqt_power_applet_qml_tests qindaqt_launcher_qml_tests qindaqt_launcher_installed_probe qindaqt_task_list_applet_qml_tests qindaqt_status_notifier_applet_qml_tests qindaqt_clipboard_applet_qml_tests qindaqt_shell_icons_qml_tests qindaqt_shell_runtime_token_tests qindaqt_shell_icon_configuration_tests qindaqt_theme_tests`: exit 0 in both profiles.
- Scrubbed `ctest -R 'qindaqt\.(launcher|task-list|global-menu|status-notifier|audio-applet|clipboard-applet|bluetooth-applet|power-applet|shell-icon|notification-center)|^qindaqt\.shell-runtime-component-closure$' --parallel 3`: Debug exit 0, 132/132; Release exit 0, 132/132, rerun on the exact committed candidate.
- Scrubbed `ctest -R '^(qindaqt\.theme-formats|qindaqt\.shell-runtime-token-publication)$'`: Debug exit 0, 2/2; Release exit 0, 2/2.
- Four documented standalone Debug configures (`launcher`, `task_list`, `bluetooth_applet`, `power_applet`) beneath the assigned build root: exit 0 each.
- Scrubbed `ctest -R '^desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)$'`: Debug exit 0, 3/3; Release exit 0, 3/3.
- Real Debug `IconImageProvider` proof with `/usr/share/icons` and the `breeze-dark` and `breeze` chains: exit 0, 68/68 declared-name/theme requests resolved to painted pixels and passed red symbolic recolor checks.
- `./tools/validate-docs`: exit 0, 146 documents/navigation entries.
- `mkdocs build --strict --site-dir <ROOT>/site`: exit 0.
- `./tools/check-source-shape`: exit 0 across 2,576 source files. The Task List QML test was split back below 500 nonblank lines; threshold warnings remain for pre-existing files and the 327-line Power QML file, all below the 500-line production ceiling.
- `git diff --check`: exit 0.
- `python3 -m json.tool` over five built-in themes and both moved theme fixtures: exit 0 for 7/7 documents.

## Negative controls and diagnostics

- Current audio contract probe against extracted `0af5d685`: expected exit 1 at missing `rows[i].isOutput === true`.
- Current Breeze coverage probe against extracted `0af5d685`: expected exit 1 at missing `applications-other`.
- Current component closure probe against extracted `0af5d685`: expected exit 1 at the parameterized, non-literal component rule.
- Pre-final diagnostic runs exposed and repaired missing Bluetooth/Power token imports, asynchronous/status fixture expectations, and the thin-panel launcher fixture. A deliberately stricter `check-source-shape --warnings-as-errors` exited 1 on repository decomposition-review thresholds; the required unmodified gate exits 0.

## Bounded caveats

- No `tests/session` nested-compositor row, host display/session bus, hardware, uinput, or network path was run; the common worker contract explicitly prohibits those rows. The safe package/sandbox/stage checks above are the complete DesktopVirtual claim for this repair.
- Breeze evidence uses the host's installed icon assets offscreen only; product tests remain deterministic through injected fixtures.
- This repair does not merge newer `main` work; the Program Manager owns reconciliation with the integrated task-fact and menu-export commits.

Independent review should attack exact candidate `7eb5372d8cc1a381dcf9fb4f1463032b1f018487`; if accepted, the Program Manager should integrate that commit.
