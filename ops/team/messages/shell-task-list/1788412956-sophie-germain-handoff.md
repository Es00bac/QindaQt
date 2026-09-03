# Sophie Germain handoff — authenticated compositor shell window actions

- Candidate commit: `11f4c0a85851376623c34bfb8cfda2ddb5383bb3`
- Candidate tree: `7a3e9b96126ba1bbe9c11b5f5113c20f47e56e85`
- Exact base: `d0b70ed80d9c6bf45b9d3b6219d1e11514514c4c`
- Branch: `worker/compositor-window-actions`
- Feature: QQ-004.10 Task list / QQ-004.07 Launcher compositor shell window actions

## Changed paths

- `compositor/dbus/org.qindaqt.CompositorShell1.xml`
- `docs/wiki/adr/0057-authenticate-shell-window-actions-by-panel-owner.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/compositor-control-v1.md`
- `docs/wiki/shell/panel-visibility.md`
- `docs/wiki/shell/task-list.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/compositor/CMakeLists.txt`
- `src/compositor/include/qindaqt/compositor/shellwindowactions.h`
- `src/compositor/kwin/kwincontrolendpoint.cpp`
- `src/compositor/kwin/kwinhybridsession.h`
- `src/compositor/kwin/kwinhybridwindowactions.cpp`
- `src/compositor/kwin/kwinshellvisibilitypublisher.cpp`
- `src/compositor/kwin/kwinshellvisibilitypublisher.h`
- `src/compositor/kwin/kwinshellwindowactions.cpp`
- `src/compositor/kwin/kwinshellwindowactions.h`
- `src/compositor/kwin/qindaqtkwinplugin.cpp`
- `src/compositor/kwin/qindaqtkwinplugin.h`
- `src/compositor/src/shellwindowactions.cpp`
- `src/shell_window_actions_client/CMakeLists.txt`
- `src/shell_window_actions_client/include/qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h`
- `src/shell_window_actions_client/include/qindaqt/shell_window_actions_client/shell_window_actions_client.h`
- `src/shell_window_actions_client/include/qindaqt/shell_window_actions_client/shell_window_actions_transport.h`
- `src/shell_window_actions_client/src/qt_shell_window_actions_transport.cpp`
- `src/shell_window_actions_client/src/shell_window_actions_client.cpp`
- `tests/CMakeLists.txt`
- `tests/compositor/ShellVisibilityTests.cmake`
- `tests/compositor/ShellWindowActionsTests.cmake`
- `tests/compositor/shellwindowactionsliveprobe.cpp`
- `tests/compositor/test_dbus_contract.py`
- `tests/compositor/test_shell_window_actions_nested.py`
- `tests/compositor/tst_shellwindowactions.cpp`
- `tests/shell_window_actions_client/CMakeLists.txt`
- `tests/shell_window_actions_client/tst_shellwindowactionsclient.cpp`
- `tests/shell_window_actions_client/tst_shellwindowactionsprivatebus.cpp`

## Acceptance evidence

All commands ran from the candidate worktree and exited 0.

- Debug configure: `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/compositor-window-actions/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`.
- Release configure: the same command with `-B /home/cabewse/work_SPaC3/builds/qindaqt/compositor-window-actions/release -DCMAKE_BUILD_TYPE=Release`.
- Focused Debug and Release builds: `cmake --build <root>/<configuration> --parallel 3 --target qindaqt_compositor qindaqt_shell_window_actions_tests qindaqt_shell_window_actions_live_probe qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests`; both exited 0.
- Final client/nested rebuild in both configurations: `cmake --build <root>/<configuration> --parallel 3 --target qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests qindaqt_shell_window_actions_live_probe`; both exited 0.
- Dependency-adjacent Debug compositor build: `cmake --build <root>/debug --parallel 3 --target tests/compositor/all`; exit 0.
- Dependency-adjacent Release build: `cmake --build <root>/release --parallel 3 --target tests/compositor/all qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests`; exit 0, 434/434 Ninja steps.
- Final focused Debug: `ctest --test-dir <root>/debug -R '^(compositor\.(shell-window-actions|kwin-shell-window-actions|dbus-contract)|qindaqt\.shell-window-actions-(client|private-bus))$' --output-on-failure --no-tests=error`; 5/5 passed.
- Final focused Release: the same selector under `<root>/release`; 5/5 passed.
- Broad Debug compositor regression: `ctest --test-dir <root>/debug -R '^compositor\.' --output-on-failure --no-tests=error`; 48/48 passed.
- Broad Release compositor/client regression: `ctest --test-dir <root>/release -R '^(compositor\.|qindaqt\.shell-window-actions-(client|private-bus)$)' --output-on-failure --no-tests=error`; 50/50 passed.
- `PYTHONPYCACHEPREFIX=<root>/pycache python3 -m py_compile tests/compositor/test_shell_window_actions_nested.py tests/compositor/test_dbus_contract.py`; exit 0.
- `./tools/validate-docs`; exit 0, 119 Markdown documents plus navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <root>/site`; exit 0.
- `./tools/check-source-shape`; exit 0 across 1,829 files. It retains the pre-existing warnings for the unchanged 500-line `tests/compositor/CMakeLists.txt` and unrelated 539-line display test; this candidate adds zero lines to the former and keeps every newly owned source below its limit.
- `git diff --check` and `git diff --cached --check`; both exited 0.

The private-bus nested row launches the exact pinned KWin 6.6.5 virtual compositor, binds a committed `dock` layer surface, proves a second local D-Bus process is unauthorized, and observes activate, minimize, unminimize, raise, and close effects on ordinary Wayland clients.

## Bounded caveats

- This candidate exposes the compositor and client boundary; task-list and launcher UI wiring remains with their owning lanes.
- Nested evidence covers ordinary windows. Hybrid-member routing is exercised through the focused policy seam and the broad Hybrid regressions, not a host desktop or `tests/session` row.
- Authentication deliberately does not defend against a compromised production shell or a process able to impersonate its committed `dock` layer role; ADR-0057 records that threat boundary.
- No host desktop, hardware, uinput, network, or host D-Bus service was touched. All live evidence used a private bus and virtual nested compositor under the assigned build root.

## Requested next action

Independent exact review then manager integration.
