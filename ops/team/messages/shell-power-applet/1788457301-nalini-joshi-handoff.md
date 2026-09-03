# Session actions candidate handoff

- Candidate commit: `437064325e7aa29b14a7dab230382ab947e71253`
- Tree: `5316d4a6a1b880ea5c9a691f9b12e76c65cb3e56`
- Exact base: `196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`
- Product commit before required main merge: `556b71d16d7ac809e8a298cdd0f8c08d7c5a4eaa`
- Merged main: `f6ba3f8acfb38e9a8e9b8c75889138867a9271f5`

## Changed paths

Sorted `base..candidate` paths follow. The manager-owned handoff/task/feature/queue/manager-record paths were inherited unchanged from the required `main` merge; Nalini authored the remaining paths.

- `docs/HANDOFF.md` (inherited from `main`)
- `docs/TASK_LIST.md` (inherited from `main`)
- `docs/wiki/adr/0070-confine-session-actions-behind-authenticated-boundaries.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/apps/power-settings.md`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/network-secret-agent.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `docs/wiki/reference/power1-v1.md`
- `docs/wiki/reference/session1-v1.md`
- `docs/wiki/shell/applet-runtime.md`
- `docs/wiki/shell/power-applet.md`
- `mkdocs.yml`
- `ops/team/features.json` (inherited from `main`)
- `ops/team/messages/shell-power-applet/1788453568-nalini-joshi-claim.md`
- `ops/team/messages/shell-power-applet/1788454866-nalini-joshi-midpoint.md`
- `ops/team/queues/first-party.md` (inherited from `main`)
- `ops/team/queues/platform.md` (inherited from `main`)
- `ops/team/queues/shell.md` (inherited from `main`)
- `ops/team/workers/claude-program-manager.md` (inherited from `main`)
- `ops/team/workers/nalini-joshi.md`
- `src/CMakeLists.txt`
- `src/apps/settings/power/CMakeLists.txt`
- `src/apps/settings/power/include/qindaqt/apps/settings_power/power_settings_model.h`
- `src/apps/settings/power/power_route_composition.cpp`
- `src/apps/settings/power/power_settings_model.cpp`
- `src/apps/settings/power/qml/PowerPage.qml`
- `src/apps/settings/power/qml/PowerSessionSection.qml`
- `src/services/session_actions/CMakeLists.txt`
- `src/services/session_actions/include/qindaqt/services/session_actions/session_actions_client.h`
- `src/services/session_actions/src/session_actions_client.cpp`
- `src/session_supervisor/CMakeLists.txt`
- `src/session_supervisor/app/main.cpp`
- `src/session_supervisor/include/qindaqt/session_supervisor/session_process_supervisor.h`
- `src/session_supervisor/include/qindaqt/session_supervisor/session_service.h`
- `src/session_supervisor/include/qindaqt/session_supervisor/supervised_process_launcher.h`
- `src/session_supervisor/org.qindaqt.Session1.xml`
- `src/session_supervisor/src/session_process_supervisor.cpp`
- `src/session_supervisor/src/session_service.cpp`
- `src/session_supervisor/src/supervised_process_launcher.cpp`
- `src/shell/CMakeLists.txt`
- `src/shell/power_applet/qml/PowerApplet.qml`
- `src/shell/power_applet/src/power_applet_controller.cpp`
- `src/shell/power_applet/src/power_applet_controller.h`
- `src/shell/runtime/powerappletcomposition.cpp`
- `src/shell/runtime/powerappletcomposition.h`
- `tests/CMakeLists.txt`
- `tests/apps/settings/power/check_boundary.cmake`
- `tests/apps/settings/power/stub_power_settings_model.h`
- `tests/apps/settings/power/tst_power_page.cpp`
- `tests/apps/settings/power/tst_power_settings_model.cpp`
- `tests/services/session_actions/CMakeLists.txt`
- `tests/services/session_actions/check_boundary.cmake`
- `tests/services/session_actions/check_boundary_negative.cmake`
- `tests/services/session_actions/tst_session_actions_client.cpp`
- `tests/session_supervisor/CMakeLists.txt`
- `tests/session_supervisor/session_plain_child_helper.cpp`
- `tests/session_supervisor/session_token_child_helper.cpp`
- `tests/session_supervisor/tst_session_process_supervisor.cpp`
- `tests/shell/power_applet/CMakeLists.txt`
- `tests/shell/power_applet/check_runtime_boundary.cmake`
- `tests/shell/power_applet/tst_power_applet_qml.cpp`

## Acceptance evidence

- Debug configure with the assigned system-KWin cache and all mandated feature/strict/test flags: exit 0.
- Release configure with the same cache/flags and `CMAKE_BUILD_TYPE=Release`: exit 0.
- Focused Debug target build (session supervisor/actions, Power settings/applet, dependency-adjacent power/session consumers, desktop probe, secret agent): exit 0; the install-closure prerequisites `qindaqt-shell-preview`, `qindaqt_global_menu_qmlplugin`, and `qindaqt_shell_launcher_qmlplugin` also built with exit 0.
- Equivalent clean Release target build: exit 0, 1,702 Ninja steps.
- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.(session|power-|settings-power-)' --output-on-failure --no-tests=error`: exit 0, 39/39 passed after the main merge.
- `ctest --test-dir <ROOT>/debug -R '^qindaqt\.shell-runtime-' --output-on-failure --no-tests=error`: exit 0, 3/3 passed after the main merge.
- `ctest --test-dir <ROOT>/debug -R '^desktop\.virtual\.(sandbox-unit|package-contract)$' --output-on-failure --no-tests=error`: exit 0, 2/2 passed after the main merge.
- The same three Release commands: exit 0, 39/39, 3/3, and 2/2 passed after the main merge.
- `./tools/validate-docs`: exit 0, 141 Markdown documents plus navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2,420 files checked; only reported existing/review-threshold warnings, including the additive 503-line shell CMake file and 298-line Power applet QML.
- `git diff --check`: exit 0.

An initial invocation of each Debug install selector failed because the narrow build omitted preview/GlobalMenu/Launcher plugin artifacts; after those exact prerequisite targets were built, both selectors passed and were rerun successfully again on the merged candidate. Product D-Bus tests used private buses with the system bus poisoned.

## Bounded caveats

This candidate claims private-bus fake coverage, offscreen `QT_FATAL_WARNINGS=1` presentation coverage, and installed-package contracts only. It does not claim host login1, host session locking, hardware, network, uinput, or nested-compositor evidence. The standalone Settings process is not the authenticated shell PID, so Session1 intentionally reports Log out unavailable there; the shell Power applet owns the authorized logout path.

Requested next action: independent exact review of `437064325e7aa29b14a7dab230382ab947e71253`, then manager integration.
