# Mary Cartwright — AppShell/global-menu second repair handoff

- Timestamp: 2026-09-03T09:44:13-06:00
- Candidate commit: `e37d90692be6ca42db112c45b1862e1d862aae7e`
- Candidate tree: `37c310817aa544b21f3fb6d0488bb113e5dece53`
- Candidate parent: `972ebc6d01e5b84fe1549df01afc85369e200fdf`
- Rejected product base: `b773ace6cd59196aaf11d3d3b40fd16cba20dc8d`
- Original lane base: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f`
- Branch: `worker/app-menu-export`
- Requested next action: independent exact review by Trachette Jackson, then manager integration.

## Outcome

Closed the remaining P1-03 defect. The transport-owned dbusmenu v4 server now
treats an empty `GetGroupProperties.ids` list as a request for every published
non-root menu item, returns those entries in stable layout pre-order, and still
applies the caller's property-name filter. The registered server regression
requires the submenu and nested action, their stable wire IDs, ordering, and
filtered labels.

The regression was compiled while the product implementation still matched
`b773ace`: `emptyGroupPropertyIdsReturnAllItems` failed with `group.size() == 0`
against an expected `2`. After the repair, the focused executable passes 5/5
in both build profiles. The earlier rejected-close, single-lineage,
complete-method-surface, registrar transition, and real File Manager
PID/window-ID mismatch closures remain green in the broad selector.

## Changed paths

- `docs/wiki/adr/0065-compose-first-party-menu-export-through-appshell.md`
- `docs/wiki/apps/application-shell.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/global-menu.md`
- `src/shell/global_menu/dbusmenu/include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_server.h`
- `src/shell/global_menu/dbusmenu/src/dbusmenu_server.cpp`
- `tests/shell/global_menu/dbusmenu/tst_dbusmenu_server.cpp`

## Evidence

Regression control before the production repair, after the new test compiled:

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug --parallel 3 --target qindaqt_global_menu_dbusmenu_server_tests && /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug/tests/shell/global_menu/dbusmenu/qindaqt_global_menu_dbusmenu_server_tests emptyGroupPropertyIdsReturnAllItems -txt
# exit 1 as required on the b773ace-equivalent source; 2 passed, 1 failed
# actual group.size() 0, expected 2
```

The first authoring attempt at that control exited 1 during compilation because
an initializer-list comma crossed the `QCOMPARE` macro; it was corrected before
the behavioral control above and is absent from the candidate.

Final Debug and Release configuration, each exit 0:

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Both retained the expected Qt `GuiPrivate` version-coupling and dependency-root
RPATH warnings; neither failed.

Final focused builds, each exit 0:

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export qindaqt_global_menu_qmlplugin tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/release --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export qindaqt_global_menu_qmlplugin tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
```

Direct server behavior after repair:

```text
/home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug/tests/shell/global_menu/dbusmenu/qindaqt_global_menu_dbusmenu_server_tests -txt
# exit 0; 5 passed, 0 failed
/home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/release/tests/shell/global_menu/dbusmenu/qindaqt_global_menu_dbusmenu_server_tests -txt
# exit 0; 5 passed, 0 failed
```

Mandated hostile-environment selectors, with fatal warnings applied to every
selected row including QML:

```text
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
# exit 0; 46/46 passed; 0 failed; 14.88 s
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/release -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
# exit 0; 46/46 passed; 0 failed; 12.99 s
```

Final static gates:

```text
./tools/validate-docs
# exit 0; 131 Markdown documents and mkdocs.yml navigation validated
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/site
# exit 0; documentation built in 1.53 s
./tools/check-source-shape
# exit 0; 2130 files checked, 0 skipped; four pre-existing warnings outside this lane
git diff --check
# exit 0, no output
git diff --check b773ace6cd59196aaf11d3d3b40fd16cba20dc8d..e37d90692be6ca42db112c45b1862e1d862aae7e
# exit 0, no output
```

No JSON changed, so `python3 -m json.tool` was not applicable. An exploratory
`clang-format --dry-run --Werror` over the two touched C++ files exited 1
because the tool's default style rejects the files' pre-existing repository
format wholesale; this is not a configured repository gate, and the required
strict compiler and source-shape gates pass.

## Bounded caveats

- No `tests/session`, host session/system D-Bus service, hardware, uinput,
  network, or nested-compositor row was run.
- Runtime D-Bus rows used their registered private `dbus-run-session`; the
  ambient session bus was removed and the system bus fenced to `/nonexistent`.
- This candidate does not claim a live installed session, foreign-toolkit
  exporter coverage, physical display/input behavior, or native-Wayland
  protocol qualification.
- The four source-shape warnings remain pre-existing and outside this lane:
  Settings Center test (583), audio-applet controller test (563), display color
  model test (539), and compositor CMake (500).
