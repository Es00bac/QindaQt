# Handoff — Shell iconography I2 icon-first panel applets

- Worker: May-Britt Moser (`may-britt-moser`), OpenAI Codex `gpt-5.6-sol`, reasoning high.
- Exact candidate commit: `0af5d685a36e8c5bb0e00fc708248e93b92ae739`.
- Candidate tree: `16dd4642d9f5a86b2b12f1f1825cac97c0703b56`.
- Exact base: `a5d78e804f28b5e11df8216ef8e0995f5d9d5191`.
- Requested next action: independent exact review then manager integration.

## Changed paths

- `data/themes/qinda-dark.json`
- `data/themes/qinda-dusk.json`
- `data/themes/qinda-high-contrast.json`
- `data/themes/qinda-light.json`
- `data/themes/qinda-macos.json`
- `docs/wiki/architecture/design-tokens.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/theme-schema-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/audio-applet.md`
- `docs/wiki/shell/bluetooth-applet.md`
- `docs/wiki/shell/clipboard-applet.md`
- `docs/wiki/shell/controls.md`
- `docs/wiki/shell/global-menu.md`
- `docs/wiki/shell/iconography.md`
- `docs/wiki/shell/launcher.md`
- `docs/wiki/shell/notification-presentation.md`
- `docs/wiki/shell/panel-surfaces.md`
- `docs/wiki/shell/power-applet.md`
- `docs/wiki/shell/status-tray.md`
- `docs/wiki/shell/task-list.md`
- `src/shell/CMakeLists.txt`
- `src/shell/IconRuntimeInstall.cmake`
- `src/shell/ShellAppletRuntimeInstall.cmake`
- `src/shell/app/shellpreviewapplication.cpp`
- `src/shell/app/shellpreviewapplication.h`
- `src/shell/audio_applet/CMakeLists.txt`
- `src/shell/audio_applet/qml/AudioApplet.qml`
- `src/shell/bluetooth_applet/CMakeLists.txt`
- `src/shell/bluetooth_applet/qml/BluetoothApplet.qml`
- `src/shell/clipboard_applet/CMakeLists.txt`
- `src/shell/clipboard_applet/qml/ClipboardPanelApplet.qml`
- `src/shell/common/shelliconconfiguration.cpp`
- `src/shell/common/shelliconconfiguration.h`
- `src/shell/global_menu/applet/qml/GlobalMenuApplet.qml`
- `src/shell/launcher/CMakeLists.txt`
- `src/shell/launcher/qml/LauncherApplet.qml`
- `src/shell/power_applet/CMakeLists.txt`
- `src/shell/power_applet/qml/PowerApplet.qml`
- `src/shell/qml/AppletChip.qml`
- `src/shell/qml/BuiltinAppletContent.qml`
- `src/shell/qml/PanelAppletColumn.qml`
- `src/shell/qml/PanelAppletRow.qml`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `src/shell/runtime/shellruntimeapplication_applets.cpp`
- `src/shell/runtime/shellruntimeapplication_tokens.cpp`
- `src/shell/runtime/tasklistappletcomposition.cpp`
- `src/shell/runtime/tasklistappletcomposition.h`
- `src/shell/status_notifier/applet/CMakeLists.txt`
- `src/shell/status_notifier/applet/qml/StatusNotifierApplet.qml`
- `src/shell/task_list/applet/CMakeLists.txt`
- `src/shell/task_list/applet/include/qindaqt/shell/task_list/applet/task_list_applet_controller.h`
- `src/shell/task_list/applet/qml/TaskListApplet.qml`
- `src/shell/task_list/applet/qml/TaskListEntryButton.qml`
- `src/shell/task_list/applet/src/task_list_applet_controller.cpp`
- `tests/session/DesktopSessionTests.cmake`
- `tests/session/DesktopVirtualAppletModules.cmake`
- `tests/session/desktop_session_matrix.py`
- `tests/shell/CMakeLists.txt`
- `tests/shell/audio_applet/CMakeLists.txt`
- `tests/shell/audio_applet/tst_audio_applet_qml.cpp`
- `tests/shell/bluetooth_applet/CMakeLists.txt`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_qml.cpp`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp`
- `tests/shell/clipboard_applet/CMakeLists.txt`
- `tests/shell/clipboard_applet/qml/tst_ClipboardProductionPanelKeyboard.qml`
- `tests/shell/clipboard_applet/qml_interactive_main.cpp`
- `tests/shell/global_menu/qml/CMakeLists.txt`
- `tests/shell/launcher/CMakeLists.txt`
- `tests/shell/launcher/run_installed_launcher.cmake`
- `tests/shell/launcher/tst_launcher_installed_probe.cpp`
- `tests/shell/launcher/tst_launcher_qml.cpp`
- `tests/shell/power_applet/CMakeLists.txt`
- `tests/shell/power_applet/tst_power_applet_qml.cpp`
- `tests/shell/qml/imports/QindaQt/Shell/AudioApplet/AudioApplet.qml`
- `tests/shell/qml/imports/QindaQt/Shell/Icons/Icon.qml`
- `tests/shell/qml/imports/QindaQt/Shell/Icons/qmldir`
- `tests/shell/qml/imports/QindaQt/Tokens/Tokens.qml`
- `tests/shell/qml/imports/QindaQt/Tokens/qmldir`
- `tests/shell/status_notifier/CMakeLists.txt`
- `tests/shell/status_notifier/applet/CMakeLists.txt`
- `tests/shell/status_notifier/applet/installed_consumer/CMakeLists.txt`
- `tests/shell/status_notifier/applet/installed_cpp_consumer.cpp`
- `tests/shell/status_notifier/applet/qml/tst_StatusNotifierApplet.qml`
- `tests/shell/status_notifier/applet/qml/tst_StatusNotifierAppletAccessibility.qml`
- `tests/shell/status_notifier/applet/qml_interactive_main.cpp`
- `tests/shell/task_list/CMakeLists.txt`
- `tests/shell/task_list/installed_task_list_consumer/CMakeLists.txt`
- `tests/shell/task_list/installed_task_list_cpp_consumer.cpp`
- `tests/shell/task_list/tst_task_list_applet_qml.cpp`
- `tests/shell/testdata/icon-themes/bad.json`
- `tests/shell/testdata/icon-themes/default-light.json`
- `tests/shell/tst_shelliconconfiguration.cpp`
- `tests/shell/verify_shell_icon_coverage.cmake`

## Acceptance evidence

- Debug and Release configured with the lane's exact Ninja/cache/strict-warning recipe: exit 0 in both profiles.
- `cmake --build <ROOT>/{debug,release} --parallel 3 --target qindaqt-shell qindaqt-shell-preview`: exit 0 in both profiles.
- `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/debug -R 'qindaqt\.(launcher|task-list|global-menu|status-notifier|audio-applet|clipboard-applet|bluetooth-applet|power-applet|shell-icon|notification-center)' --output-on-failure --no-tests=error --parallel 3`: exit 0, 130/130 passed.
- The identical Release selector: exit 0, 130/130 passed.
- `ctest -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)'` under the same scrubbed environment: Debug exit 0, 3/3 passed; Release exit 0, 3/3 passed.
- Before every live contained row, `pgrep -f 'kwin_wayland.*qindaqt-parent-way[l]and'` returned no process. Every row ran serially with the private runtime opt-in, host display variables unset, an unreachable system-bus address, and the test-owned bubblewrap/private-bus/headless-Weston topology.
- `desktop.virtual.boot.1080p`: Debug exit 0, 2/2 passed; Release exit 0, 2/2 passed.
- `desktop.virtual.panel-visibility.single-1080p`: Debug exit 0, 2/2 passed; Release exit 0, 2/2 passed.
- `desktop.virtual.panel-visibility.single-wuxga`: Debug exit 0, 2/2 passed; Release exit 0, 2/2 passed.
- `desktop.virtual.interactive.1080p`: Debug exit 0, 2/2 passed; final Release rerun exit 0, 2/2 passed (interactive row 12.88 seconds).
- Added the existing `single-1080p-125` light-theme scenario to the closed executable matrix. `desktop.virtual.interactive.matrix.single-1080p-125`: Debug exit 0, 2/2 passed; Release exit 0, 2/2 passed.
- `./tools/validate-docs`: exit 0, 146 documents/navigation entries validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`: exit 0.
- `./tools/check-source-shape`: exit 0 across 2,573 source files. The owned `src/shell/CMakeLists.txt` was decomposed from 599 nonblank lines into a cohesive install helper and is no longer in the warning set.
- `git diff --check`: exit 0.
- `python3 -m json.tool data/themes/*.json`: exit 0 for all five changed theme documents.

## Visual evidence

- Dark: `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/shell-icon-first-applets/after-dark-1080p.png`, 1920x1080, SHA-256 `31fa83aa11790e182827d302458e749dc3c6804047a028556a630c6ebface348`.
- Light: `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/shell-icon-first-applets/after-light-1080p.png`, 1920x1080, SHA-256 `19b03b3b8601a004fab6aa432079b4e820cc32116a8cda3d0a63a48051f17ed4`.
- Manual inspection found compact icon chips in the declared panels with no rejected `Applications`, `Audio`, `Status tray`, `grouped task list`, or `Limited` panel labels and no visible panel-edge clipping. The screenshot interaction intentionally leaves the notification-center popup open.
- A Pillow pixel inspection asserted exact 1920x1080 dimensions and non-empty, multi-color content wholly sampled inside the authoritative dark top/bottom and light top panel rectangles: exit 0 (`dark PASS panels 2`, `light PASS panels 1`). Per-chip containment, hidden panel labels, accessible names, vertical layout, and resolved-or-placeholder icon sources are enforced structurally by the warning-fatal QML rows above.
- One preliminary direct attempt to select `single-1080p-125` before approving it in the closed executable matrix exited 1 with the expected fail-closed `not approved` diagnostic. After the additive selector/docs change and reconfiguration, both registered rows passed as reported.

## Bounded caveats

- Evidence is software-rendered and contained (`bwrap-pid-network-ipc`, private D-Bus, headless parent compositor). It makes no GPU, physical-input, host-session, hardware-service, network, uinput, or nested-host-desktop claim.
- System theme assets are deliberately not assumed inside the staged sandbox. Missing names render I1's typed placeholder; the production/preview runtime still searches the selected theme, its dark/light default, and `hicolor` in order.
- Task buttons render whatever compositor task facts arrive. This candidate consumes the existing app-id boundary and resolves desktop-entry icons, but does not claim or modify the concurrently owned compositor producer/operations implementation.
- Clock and notification-center presentation remain behaviorally unchanged apart from shared panel alignment. The open notification-center content visible in captures is not an applet iconography baseline.

Independent exact review should attack candidate `0af5d685a36e8c5bb0e00fc708248e93b92ae739`; if accepted, the Program Manager should integrate that exact commit.
