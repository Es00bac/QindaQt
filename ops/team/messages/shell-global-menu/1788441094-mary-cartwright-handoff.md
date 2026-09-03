# Mary Cartwright — first-party AppShell global-menu export handoff

## Candidate

- Candidate commit: `59353bf431b3a9d19f20e9db23617cb839fd1dda`.
- Candidate tree: `e69799515d99b809daa7e8be7cb1bfe661e6ac35`.
- Exact base: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f`.
- Branch: `worker/app-menu-export`.

## Changed paths

- `docs/wiki/adr/0065-compose-first-party-menu-export-through-appshell.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/apps/application-shell.md`
- `docs/wiki/apps/file-manager.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/global-menu.md`
- `mkdocs.yml`
- `src/app_shell/CMakeLists.txt`
- `src/app_shell/menu_export/CMakeLists.txt`
- `src/app_shell/menu_export/include/qindaqt/app_shell/menu_export/application_menu_export.h`
- `src/app_shell/menu_export/include/qindaqt/app_shell/menu_export/window_menu_identity.h`
- `src/app_shell/menu_export/src/application_menu_export.cpp`
- `src/app_shell/menu_export/src/dbusmenu_export_object.cpp`
- `src/app_shell/menu_export/src/dbusmenu_export_object_p.h`
- `src/app_shell/menu_export/src/qt_window_menu_identity.cpp`
- `src/apps/file_manager/CMakeLists.txt`
- `src/apps/file_manager/app_shell/file_manager_action_catalog.cpp`
- `src/apps/file_manager/app_shell/file_manager_action_catalog.h`
- `src/apps/file_manager/main.cpp`
- `tests/app_shell/CMakeLists.txt`
- `tests/app_shell/check_app_shell_source_policy.cmake`
- `tests/app_shell/tst_application_menu_export.cpp`
- `tests/shell/global_menu/runtime_composition/CMakeLists.txt`
- `tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp`

## Acceptance evidence

All commands ran from `/home/cabewse/work_SPaC3/container-wm-workers/app-menu-export`.

- Debug configure, exit 0:

  ```sh
  cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

- Release configure, exit 0: the same command with `-B /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/release -DCMAKE_BUILD_TYPE=Release`.
- Focused strict builds in Debug and Release, exit 0, covered `qindaqt_app_shell`, `qindaqt_app_shellplugin`, `qindaqt_app_shell_menu_export`, `qindaqt-file-manager`, `qindaqt-shell`, all global-menu libraries, and every AppShell/File Manager/global-menu test executable selected below. The final source-touch rebuild also ran in both profiles with:

  ```sh
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/<profile> --parallel 3 --target qindaqt_app_shell_menu_export qindaqt-file-manager qindaqt_app_shell_menu_export_tests qindaqt_file_manager_menu_export_tests
  ```

  The final Debug dependency-adjacent target replay additionally exited 0 after Ninja warned that it was recovering a premature end in its local `.ninja_log`; the recovery rebuilt 453 steps and did not change sources.

- Debug selected tests, exit 0, 44/44 passed:

  ```sh
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
  ```

- Release selected tests, exit 0, 44/44 passed: the same command with `--test-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/release`.
- `./tools/validate-docs`: exit 0, 131 Markdown documents and navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2128 files checked; four reported decomposition-review warnings are pre-existing paths outside this lane.
- `git diff --check`: exit 0.
- No JSON changed, so no JSON parser gate applied.

The first full Debug selector exposed two File Manager offscreen rows crashing when their QML root was explicitly destroyed before the retained composition. The implementation was repaired to weakly track the window and stop on destruction; the complete Debug selector was then rerun and passed 44/44. No failing result is being presented as acceptance evidence.

## Bounded caveats

- The candidate proves standard dbusmenu traffic, X window-id truth through an injected identity publisher, native-Wayland no-numeric-id behavior, registrar owner replacement/loss, close teardown, exactly-once activation, and a real File Manager process consumed by production shell composition on a private bus.
- It does not claim a host session, nested compositor, native Wayland protocol round trip, physical display/hardware, GTK exporter, Text Editor, or Terminal wiring.
- Native Wayland publication is intentionally coupled to Qt 6.11's private KDE appmenu platform hook; ADR-0065 confines and records that rebuild-time dependency. The production shell remains the authority that authenticates the announced service/path and PID.
- Local menu visibility remains authoritative because publication is not authenticated shell-hosted proof.

## Requested next action

Independent exact review of `59353bf431b3a9d19f20e9db23617cb839fd1dda`, then Program Manager integration.
