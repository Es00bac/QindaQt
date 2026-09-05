# Desktop polish handoff — Frances Arnold

- Timestamp: 2026-09-05T00:55:24-06:00
- Candidate commit: `511ac8623051b78adfb54634d9a5fce0c466005f`
- Candidate tree: `142c77ba657c056f00655395789fee1ddbe4c5fd`
- Exact base: `6a2019aa8dd6110054cca2ce0b4e8b23a9662251`
- Branch/worktree: `worker/desktop-polish` at `/home/cabewse/work_SPaC3/container-wm-workers/desktop-polish`

## Outcome

- DesktopVirtual stages all four first-party desktop entries; production task-list composition resolves their `Icon=` names through the configured theme roots, and ShellDevelopment evidence fails closed on unresolved first-party task icons.
- Compositor task facts carry normal/non-normal window type and application/bound-shell ownership. T0 excludes either non-normal or bound-shell entries before output/workspace scope, so shell popups do not become tasks.
- Runtime profile compatibility maps the launcher alias and removes a redundant legacy task-list alias when the canonical applet is present. Empty status-notifier content also has zero panel extent and no unavailable marker.
- Notification quieting owns an exact-key Settings1 client. A fresh profile qualifies as available, baseline-confirmed, toggleable, and off rather than reporting the setting unavailable.

## Changed paths

- `docs/wiki/shell/iconography.md`
- `docs/wiki/shell/notification-presentation.md`
- `docs/wiki/shell/status-tray.md`
- `docs/wiki/shell/task-list.md`
- `src/compositor/include/qindaqt/compositor/shelltaskfacts.h`
- `src/compositor/kwin/kwinshelltaskfacts.cpp`
- `src/compositor/kwin/kwinshelltaskfacts.h`
- `src/compositor/kwin/qindaqtkwinplugin.cpp`
- `src/compositor/src/shelltaskfacts.cpp`
- `src/compositor/src/shelltaskfactscodec.cpp`
- `src/shell/CMakeLists.txt`
- `src/shell/qml/AppletChip.qml`
- `src/shell/qml/PanelAppletColumn.qml`
- `src/shell/qml/PanelAppletRow.qml`
- `src/shell/runtime/runtimepanelappletcompatibility.cpp`
- `src/shell/runtime/runtimepanelappletcompatibility.h`
- `src/shell/runtime/runtimepanelwindowfactory.cpp`
- `src/shell/runtime/runtimepanelwindowfactory.h`
- `src/shell/runtime/shelldevelopmentevidence.cpp`
- `src/shell/runtime/shelldevelopmentevidence.h`
- `src/shell/runtime/shellruntimeapplication.cpp`
- `src/shell/runtime/shellruntimeapplication.h`
- `src/shell/runtime/shellruntimeapplication_applets.cpp`
- `src/shell/runtime/shellruntimeapplication_development.cpp`
- `src/shell/runtime/tasklistappletcomposition.cpp`
- `src/shell/runtime/tasklistappletcomposition.h`
- `src/shell/status_notifier/applet/qml/StatusNotifierApplet.qml`
- `src/shell/task_list/applet/include/qindaqt/shell/task_list/applet/task_list_applet_controller.h`
- `src/shell/task_list/applet/src/task_list_applet_controller.cpp`
- `src/shell/task_list/include/qindaqt/shell/task_list/task_list_types.h`
- `src/shell/task_list/producer/src/task_list_wire.cpp`
- `src/shell/task_list/src/task_list_filter.cpp`
- `src/shell/task_list/src/task_list_grouping.cpp`
- `tests/compositor/tst_shelltaskfacts.cpp`
- `tests/session/CMakeLists.txt`
- `tests/session/DesktopNotificationShellReadinessTests.cmake`
- `tests/session/DesktopSessionTests.cmake`
- `tests/session/DesktopVirtualAppletModules.cmake`
- `tests/session/desktop_session_interactive.py`
- `tests/session/desktop_session_notification_shell.py`
- `tests/session/desktop_session_package_contract.py`
- `tests/session/desktop_session_shell_fixtures.py`
- `tests/session/desktop_session_shell_polish.py`
- `tests/session/desktopnotificationshellpolish.cpp`
- `tests/session/desktopnotificationshellpolish.h`
- `tests/session/desktopnotificationshellpolishfixtures.cpp`
- `tests/session/desktopnotificationshellpolishfixtures.h`
- `tests/session/desktopnotificationshellreadiness.cpp`
- `tests/session/fixtures/desktop_session/probe-observed-fallback-1080p.json`
- `tests/session/fixtures/desktop_session/probe-ready-1080p.json`
- `tests/session/test_desktop_session_contract_unit.py`
- `tests/session/test_desktop_session_interactive_unit.py`
- `tests/session/test_desktop_session_matrix_unit.py`
- `tests/session/test_desktop_session_package.py`
- `tests/session/test_desktop_session_package_contract_unit.py`
- `tests/session/tst_desktopnotificationshellreadiness.cpp`
- `tests/shell/CMakeLists.txt`
- `tests/shell/status_notifier/applet/qml/tst_StatusNotifierProductionPanelKeyboard.qml`
- `tests/shell/task_list/task_list_producer_test_support.h`
- `tests/shell/task_list/tst_task_list_applet_controller.cpp`
- `tests/shell/task_list/tst_task_list_scope_filter.cpp`
- `tests/shell/tst_runtimepanelappletcompatibility.cpp`

## Verification evidence

All commands ran from the lane worktree. Display variables and the host session bus were unset for test commands; `DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent` was explicit. Product tests made no network, hardware, host-bus, or uinput claim.

- Exact Debug configure from the worker brief, with build directory `/home/cabewse/work_SPaC3/builds/qindaqt/desktop-polish/debug`: exit 0.
- Exact Release configure from the worker brief, with build directory `/home/cabewse/work_SPaC3/builds/qindaqt/desktop-polish/release`: exit 0.
- `cmake --build <config> --parallel 3` over the owned shell, recursive shell-applet, compositor, DesktopVirtual probe, notification-shell-readiness, runtime-options, status-notifier QML, and panel-visibility-probe targets: Debug exit 0; Release exit 0. Both used strict warnings enabled by the exact configure.
- `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent TMPDIR=<config>/tmp ctest --test-dir <config> -R '^qindaqt\.(shell-|applet|task-list-|status-notifier-|launcher|notification-center|compositor)|^desktop\.virtual\.(sandbox-unit|package-contract|stage-closure|notification-shell-readiness-unit)' --output-on-failure --no-tests=error`: Debug exit 0, 114/114 passed; Release exit 0, 114/114 passed.
- The same isolated environment with `ctest --test-dir <config> -R '^compositor\.shell-task-facts$' --output-on-failure --no-tests=error`: Debug exit 0, 1/1 passed; Release exit 0, 1/1 passed.
- `pgrep -af 'kwin_wayland.*qindaqt-parent-way[l]and'`: no matching process before each private nested run and none after teardown.
- The same isolated environment plus `QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop`, with `ctest --test-dir <config> -j1 -R '^(desktop\.virtual\.boot\.1080p|desktop\.virtual\.panel-visibility\.single-.*|desktop\.virtual\.interactive\.1080p|compositor\.kwin-shell-window-actions)$' --output-on-failure --no-tests=error`: Debug exit 0, 6/6 passed; Release exit 0, 6/6 passed. The six rows are the five requested rows plus the automatically required package-contract fixture.
- `./tools/validate-docs`: exit 0, 147 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/desktop-polish/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2,598 source files checked; warnings were unchanged out-of-lane review-threshold files, and every changed hand-written file is below its hard limit.
- `git diff --check`: exit 0.
- `python3 -m json.tool tests/session/fixtures/desktop_session/probe-ready-1080p.json > /dev/null` and the same command for `probe-observed-fallback-1080p.json`: both exit 0.

Repair-loop evidence retained for truthful context: the first empty-tray hostile row failed because its parent row forced a 40-pixel height; after preserving zero extent in both panel orientations, it passed and is included in both 114/114 matrices. The first Debug selector replay reported 112 passes, one failed sandbox fixture, and one unbuilt runtime-options executable; the canonical evidence fixtures and explicit target closure were repaired before the clean 114/114 replay. Nested rows first skipped without the required private-lane opt-in, then the two panel rows exposed an unbuilt visibility probe; after building that target, both final Debug and Release nested replays were clean at 6/6.

## Capture

- `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/desktop-polish/after-1080p.png`
- SHA-256: `b83818d494182d736669aa272b05258f9b5f47277fe9ed99d1f88b064d6b5ded`
- The Release interactive capture shows exactly Settings and Text Editor in the smart-shelf task list with distinct resolved icons; no `qindaqt-shell` task; no trailing white/amber shelf chip; a live launcher without a degraded dot; and an available, off Do Not Disturb switch with no red unavailable message. The open notification center truthfully shows no notifications.

## Bounded caveats

- Evidence is confined to injected fakes, private buses, offscreen QML, and the explicitly authorized headless nested parent/child compositor rows. It does not claim host desktop, hardware, host D-Bus, network, uinput, or third-party application icon coverage.
- The unchanged command-bar placeholders for not-yet-implemented profile applets are outside this lane; no new applet or D-Bus surface was introduced.

Requested next action: independent exact review then manager integration.
