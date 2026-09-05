# Donna Strickland — compositor task-fact handoff

- Candidate commit: `fa0e6d9cc522559ee17582c97816e58aaab10f42`
- Candidate tree: `82ea8a1d984a69468572c211bb5cd36c7f573659`
- Exact base: `86ad7c3854a5e35efb0a3b3e65444c432448fa0e`

## Outcome

`CompositorShell1` now publishes one bounded, authenticated, generation-fenced
task-fact snapshot plus a coalesced directed change signal. T1 consumes it
without polling, fences operations to the accepted owner/generation, and
degrades on owner loss or incoherent input. `ShellDevelopment1.Snapshot`
publishes `taskList` phase/generation/represented-window count, and the private
1080p boot row requires `ready` with at least one real compositor window.

## Changed paths

- `compositor/dbus/org.qindaqt.CompositorShell1.xml`
- `docs/wiki/adr/0072-publish-atomic-authenticated-task-facts.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/compositor-control-v1.md`
- `docs/wiki/shell/task-list.md`
- `mkdocs.yml`
- `src/compositor/CMakeLists.txt`
- `src/compositor/include/qindaqt/compositor/shelltaskfacts.h`
- `src/compositor/kwin/kwinhybridpublicstate.cpp`
- `src/compositor/kwin/kwinhybridsession.h`
- `src/compositor/kwin/kwinshelltaskfacts.cpp`
- `src/compositor/kwin/kwinshelltaskfacts.h`
- `src/compositor/kwin/kwinshellwindowactions.cpp`
- `src/compositor/kwin/kwinshellwindowactions.h`
- `src/compositor/kwin/qindaqtkwinplugin.cpp`
- `src/compositor/kwin/qindaqtkwinplugin.h`
- `src/compositor/src/shelltaskfacts.cpp`
- `src/compositor/src/shelltaskfactscodec.cpp`
- `src/shell/runtime/shelldevelopmentevidence.cpp`
- `src/shell/runtime/shelldevelopmentevidence.h`
- `src/shell/runtime/shellruntimeapplication_development.cpp`
- `src/shell/runtime/tasklistappletcomposition.cpp`
- `src/shell/task_list/applet/include/qindaqt/shell/task_list/applet/task_list_applet_controller.h`
- `src/shell/task_list/applet/src/task_list_applet_controller.cpp`
- `src/shell/task_list/producer/CMakeLists.txt`
- `src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/qt_task_list_producer_transport.h`
- `src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_facts_producer.h`
- `src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_operation_authority.h`
- `src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_producer_transport.h`
- `src/shell/task_list/producer/include/qindaqt/shell/task_list/producer/task_list_wire.h`
- `src/shell/task_list/producer/src/qt_task_list_producer_transport.cpp`
- `src/shell/task_list/producer/src/task_list_facts_producer.cpp`
- `src/shell/task_list/producer/src/task_list_wire.cpp`
- `tests/compositor/ShellWindowActionsTests.cmake`
- `tests/compositor/test_dbus_contract.py`
- `tests/compositor/tst_shelltaskfacts.cpp`
- `tests/session/desktop_session_interactive.py`
- `tests/session/desktop_session_notification_shell.py`
- `tests/session/desktop_session_shell_fixtures.py`
- `tests/session/desktopnotificationshellreadiness.cpp`
- `tests/session/fixtures/desktop_session/probe-observed-fallback-1080p.json`
- `tests/session/fixtures/desktop_session/probe-ready-1080p.json`
- `tests/session/test_desktop_session_interactive_unit.py`
- `tests/session/test_desktop_session_matrix_unit.py`
- `tests/session/test_desktop_session_readiness_unit.py`
- `tests/session/tst_desktopnotificationshellreadiness.cpp`
- `tests/shell/task_list/CMakeLists.txt`
- `tests/shell/task_list/installed_task_list_consumer/CMakeLists.txt`
- `tests/shell/task_list/installed_task_list_cpp_consumer.cpp`
- `tests/shell/task_list/run_installed_task_list_applet.cmake`
- `tests/shell/task_list/task_list_operation_test_support.h`
- `tests/shell/task_list/task_list_producer_test_support.h`
- `tests/shell/task_list/tst_task_list_applet_controller.cpp`
- `tests/shell/task_list/tst_task_list_facts_producer.cpp`
- `tests/shell/task_list/tst_task_list_qt_transports.cpp`
- `tests/shell/task_list/tst_task_list_wire.cpp`

## Acceptance evidence

- Exact prescribed CMake configure commands for
  `/home/cabewse/work_SPaC3/builds/qindaqt/compositor-task-facts/{debug,release}`
  with the system-KWin initial cache and strict warnings: exit 0 in both.
- `cmake --build <config> --parallel 3 --target qindaqt_task_list_applet_controller_tests qindaqt-desktop-notification-shell-readiness-tests qindaqt-desktop-session-probe qindaqt-shell`:
  exit 0 in Debug and Release after the final represented-window correction.
  Earlier strict focused builds completed the compositor task-fact, KWin
  plugin/action, producer/transport, installed-package, and production-shell
  target closure in both configurations.
- `ctest --test-dir <config> -R '^(qindaqt\.task-list-|compositor\.(shell-task-facts|shell-window-actions|shell-window-identity|dbus-contract)|qindaqt\.shell-window-actions-(client|private-bus)|desktop\.virtual\.(sandbox-unit|notification-shell-readiness-unit))' --output-on-failure --no-tests=error`:
  31/31 passed, exit 0, in Debug and Release.
- Post-repair `ctest --test-dir <config> -R '^(compositor\.shell-task-facts|qindaqt\.task-list-(wire|facts-producer|applet-installed-package))$' --output-on-failure --no-tests=error`:
  4/4 passed, exit 0, in Debug and Release.
- `ctest --test-dir <config> -R '^compositor\.kwin-shell-window-actions$' --output-on-failure --no-tests=error`:
  1/1 passed, exit 0, in Debug and Release.
- Final `ctest --test-dir <config> -R '^(qindaqt\.task-list-applet-controller|desktop\.virtual\.(sandbox-unit|notification-shell-readiness-unit))$' --output-on-failure --no-tests=error`:
  3/3 passed, exit 0, in Debug and Release.
- Final `env QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop ctest --test-dir <config> -R '^desktop\.virtual\.boot\.1080p$' --output-on-failure --no-tests=error`:
  2/2 passed (package fixture plus boot), exit 0, in Debug and Release. The
  authenticated snapshot was `taskList.phase=ready` with a nonzero real window
  count after Settings and Editor launched.
- The equivalent private-runtime `desktop.virtual.panel-visibility.single-1080p`
  and `single-wuxga` selectors each passed 2/2 including the package fixture,
  exit 0, in Debug and Release.
- `./tools/validate-docs`: exit 0; 145 Markdown documents validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/compositor-task-facts/site`:
  exit 0.
- `./tools/check-source-shape`: exit 0; 2552 files checked, with only existing
  review-threshold warnings.
- `git diff --check`: exit 0.
- `python3 -m json.tool` on each changed JSON fixture: exit 0 for both.

Failed intermediate evidence was repaired before the results above: the first
installed-package row exposed a missing staged compositor archive; early boot
runs exposed missing staged session/global-menu closure and a bounded
stable-owner `Snapshot` startup race; source shape caught two oversized
readiness functions. Each affected final row was rerun successfully.

## Bounded caveats

- Evidence is limited to injected fakes, private D-Bus, and contained nested
  compositor rows. It makes no host-display, host-bus, hardware, uinput, or
  network claim.
- This additive boundary does not replace public `Compositor1` inventories or
  add hybrid-container page transactions. Managed task roles are currently
  normal toplevels at the existing KWin admission boundary.

Requested next action: independent exact review then manager integration.
