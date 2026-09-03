# Sophie Germain handoff — authenticated active-window identity facts

- Handoff time: `2026-09-03T01:03:14-06:00`
- Exact candidate commit: `7e263bd0549d4ed64e8f15e6dac7709ccdfc7120`
- Candidate tree: `7c71661218b0e3fd10c2f9d3f76f6f95d39ffcfe`
- Exact base: `135fe652db422564b8eeb3dc85bfdf1f679c54eb`
- Branch: `worker/compositor-window-identity`
- Feature: QQ-004 Global Menu compositor active-window identity facts

## Changed paths

- `compositor/dbus/org.qindaqt.CompositorShell1.xml`
- `docs/wiki/adr/0062-project-authenticated-active-window-identity.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/compositor-control-v1.md`
- `docs/wiki/shell/global-menu.md`
- `mkdocs.yml`
- `src/compositor/CMakeLists.txt`
- `src/compositor/include/qindaqt/compositor/shellwindowidentity.h`
- `src/compositor/kwin/kwinshellwindowactions.cpp`
- `src/compositor/kwin/kwinshellwindowactions.h`
- `src/compositor/kwin/kwinshellwindowidentity.cpp`
- `src/compositor/kwin/kwinshellwindowidentity.h`
- `src/compositor/kwin/qindaqtkwinplugin.cpp`
- `src/compositor/kwin/qindaqtkwinplugin.h`
- `src/compositor/src/shellwindowidentity.cpp`
- `src/shell_window_actions_client/include/qindaqt/shell_window_actions_client/qt_shell_window_actions_transport.h`
- `src/shell_window_actions_client/include/qindaqt/shell_window_actions_client/shell_window_actions_client.h`
- `src/shell_window_actions_client/include/qindaqt/shell_window_actions_client/shell_window_actions_transport.h`
- `src/shell_window_actions_client/src/qt_shell_window_actions_transport.cpp`
- `src/shell_window_actions_client/src/shell_window_actions_client.cpp`
- `tests/compositor/ShellWindowActionsTests.cmake`
- `tests/compositor/shellwindowactionsliveclients.cpp`
- `tests/compositor/shellwindowactionsliveclients.h`
- `tests/compositor/shellwindowactionsliveprobe.cpp`
- `tests/compositor/test_dbus_contract.py`
- `tests/compositor/tst_shellwindowidentity.cpp`
- `tests/shell_window_actions_client/tst_shellwindowactionsclient.cpp`
- `tests/shell_window_actions_client/tst_shellwindowactionsprivatebus.cpp`

## Outcome

`CompositorShell1` now publishes a schema-1 active-window identity snapshot only after the existing exact panel-owner PID authentication. It carries its own epoch/revision lineage, the action/visibility revision sampled with the facts, the KWin UUID, credentials-derived Wayland or XRes-derived XWayland PID, exact X11 AppMenu window id when applicable, and a valid paired KDE appmenu service/path announcement. Every unavailable fact is typed `null`; native Wayland never receives an invented numeric registrar id. The no-payload invalidation is targeted to the last authenticated shell bus owner rather than broadcast.

The existing exact-owner shell window-actions client reads and refreshes identity on the same transport and owner binding. It withdraws facts on invalidation, owner loss, timeout, malformed input, revision regression, or equal-revision collision. ADR-0062 and the Global Menu consumption contract require registrar/provider credentials to be joined back to these compositor facts; the registrar itself remains untrusted.

## Acceptance evidence

All final acceptance commands ran from the lane worktree and exited `0`. `<ROOT>` below is `/home/cabewse/work_SPaC3/builds/qindaqt/compositor-window-identity`.

- Debug configure: `cmake -S . -B <ROOT>/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`.
- Release configure: the exact same command with `-B <ROOT>/release -DCMAKE_BUILD_TYPE=Release`.
- Focused final builds in both profiles: `cmake --build <ROOT>/<profile> --parallel 3 --target qindaqt_compositor qindaqt_shell_window_identity_tests qindaqt_shell_window_actions_live_probe qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests`; exit `0` in Debug and Release.
- Dependency-adjacent Debug compositor graph: `cmake --build <ROOT>/debug --parallel 3 --target tests/compositor/all`; exit `0`, 261/261 Ninja steps.
- Dependency-adjacent Release compositor/client graph: `cmake --build <ROOT>/release --parallel 3 --target tests/compositor/all qindaqt_shell_window_actions_client_tests qindaqt_shell_window_actions_private_bus_tests`; exit `0`, 441/441 Ninja steps.
- Debug compositor: `ctest --test-dir <ROOT>/debug -R '^compositor\.' --output-on-failure --no-tests=error --parallel 1`; 49/49 passed.
- Release compositor: `ctest --test-dir <ROOT>/release -R '^compositor\.' --output-on-failure --no-tests=error --parallel 1`; 49/49 passed.
- Debug client: `ctest --test-dir <ROOT>/debug -R '^qindaqt\.shell-window-actions-(client|private-bus)$' --output-on-failure --no-tests=error`; 2/2 passed.
- Release client: `ctest --test-dir <ROOT>/release -R '^qindaqt\.shell-window-actions-(client|private-bus)$' --output-on-failure --no-tests=error`; 2/2 passed.
- The nested `compositor.kwin-shell-window-actions` row ran serially inside both broad selectors and passed. It compares two real child PIDs, a typed-null native-Wayland AppMenu id, and the exact XWayland native window id, and rejects an unauthenticated identity read.
- `./tools/validate-docs`; exit `0`, 125 Markdown documents plus navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`; exit `0`.
- `./tools/check-source-shape`; exit `0`, 1,954 files checked. It reports only the pre-existing threshold/over-limit warnings in unrelated files and `tests/compositor/CMakeLists.txt`; every source added or expanded by this candidate is below 500 non-blank lines and every header below 325.
- `git diff --check` and `git diff --cached --check`; exit `0`.
- No JSON file changed, so the JSON parse gate was not applicable.

Repair-loop evidence is retained explicitly: early strict Debug builds exited `1` on a signed-character warning and use of the wrong Qt targeted-signal API; both defects were corrected. The nested row initially exited `8` when a Wayland-native id was incorrectly constrained to 32 bits, then again when identity did not follow a later visibility fence; the final publisher subscribes to visibility changes and both profile rows pass. The first broad Debug selector was interrupted with exit `130` after it revealed unbuilt legacy compositor test executables; `tests/compositor/all` was then built and every final broad row passed.

## Bounded caveats

- This candidate supplies the compositor/client proof boundary and consumption contract; it does not instantiate the blocked Global Menu G2 production composition or applet.
- Native Wayland has no numeric AppMenu registrar id in this contract. It uses a valid paired KDE appmenu service/path announcement; absent or malformed announcements fail closed.
- The live row proves real Wayland/XWayland PID and X11-id truth but its probe clients do not announce an appmenu endpoint; paired announcement validation is covered by deterministic unit/codec tests.
- The standard AppMenu registrar remains unauthenticated. A local peer can lodge bogus registrations, and a compromised application can export a malicious menu for its own window. Composition must still resolve the exact provider owner and compare daemon credentials around a stable identity revision.
- No host desktop, host session bus, `tests/session`, hardware, uinput, physical display/GPU, or network product test was run. Live evidence used a private bus and virtual KWin 6.6.5 under the assigned build root.

## Requested next action

Independent exact review then manager integration.
