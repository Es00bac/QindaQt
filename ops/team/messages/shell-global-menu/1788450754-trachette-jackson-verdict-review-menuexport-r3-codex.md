# Trachette Jackson — final AppShell/global-menu exact-candidate recheck

- Persona: Trachette Jackson, independent shell/application reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `e37d90692be6ca42db112c45b1862e1d862aae7e`
- Tree SHA: `37c310817aa544b21f3fb6d0488bb113e5dece53`
- Parent SHA: `972ebc6d01e5b84fe1549df01afc85369e200fdf`
- Base SHA: `b773ace6cd59196aaf11d3d3b40fd16cba20dc8d` (rejected product ancestor)
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/app-menu-export-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex`

## Findings ledger

### P0

None.

### P1

None. P1-03 is closed.

### P2

None.

### P3

None.

## Recheck evidence

- `DbusMenuServer::GetGroupProperties` now treats an empty ID list as an
  all-items request, walks the validated transport layout's non-root children
  in stable pre-order, and applies the caller's property-name filter to every
  returned entry (`src/shell/global_menu/dbusmenu/src/dbusmenu_server.cpp:66-76,180-204`).
- The registered `qindaqt.global-menu-dbusmenu-server` executable contains the
  non-vacuous `emptyGroupPropertyIdsReturnAllItems` row. It requires both the
  submenu and nested action, their wire IDs, stable order, and label-only
  properties (`tests/shell/global_menu/dbusmenu/tst_dbusmenu_server.cpp:98-123`).
  Before the candidate rebuild, the preserved `b773ace` scratch control for
  the same call/assertion printed `publishedItems=2 emptyIdsResult=0` and exited
  1. After rebuilding against `e37d906`, it printed
  `publishedItems=2 emptyIdsResult=2` and exited 0. The registered server test
  passed 5/5 in both Debug and Release.
- Earlier closures stayed closed. The AppShell direct private-bus executable
  passed 7/7, including rejected-close retention and the complete v4 method
  surface. The real File Manager composition executable passed 5/5, including
  matching, mismatched-PID, and mismatched-window-ID cases. The source-policy
  scan found no local lineage issuer, canonical exporter import, or ambient
  session-bus lookup in `src/app_shell/menu_export`.

## Commands and results

### Immutable tree

```text
git rev-parse HEAD
# e37d90692be6ca42db112c45b1862e1d862aae7e
git rev-parse HEAD^{tree}
# 37c310817aa544b21f3fb6d0488bb113e5dece53
git rev-parse HEAD^
# 972ebc6d01e5b84fe1549df01afc85369e200fdf
git rev-parse b773ace
# b773ace6cd59196aaf11d3d3b40fd16cba20dc8d
git status --porcelain
# exit 0, no output before and after review
```

### Configure and focused build

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export qindaqt_global_menu_qmlplugin tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
# exit 0; focused incremental build completed
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export qindaqt_global_menu_qmlplugin tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
# exit 0; focused incremental build completed
```

Both configurations retained the expected Qt `GuiPrivate` version-coupling and
dependency-root RPATH warnings; neither failed.

### Focused and direct tests

```text
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
# exit 0; 46/46 passed; 0 failed; 15.42 s
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
# exit 0; 46/46 passed; 0 failed; 13.31 s
/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug/tests/shell/global_menu/dbusmenu/qindaqt_global_menu_dbusmenu_server_tests -txt
# exit 0; 5 passed, 0 failed
/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release/tests/shell/global_menu/dbusmenu/qindaqt_global_menu_dbusmenu_server_tests -txt
# exit 0; 5 passed, 0 failed
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug/tests/app_shell/qindaqt_app_shell_menu_export_tests -txt
# exit 0; 7 passed, 0 failed
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug/tests/shell/global_menu/runtime_composition/qindaqt_file_manager_menu_export_tests -txt
# exit 0; 5 passed, 0 failed
```

No `tests/session`, host session/system service, hardware, uinput, network, or
nested-compositor row was run. Runtime D-Bus rows used their registered private
`dbus-run-session`; the ambient session bus was removed and the system bus was
fenced to `/nonexistent`.

### Static gates

```text
./tools/validate-docs
# exit 0; 131 Markdown documents and mkdocs.yml navigation validated
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/site
# exit 0; documentation built in 1.61 s
./tools/check-source-shape
# exit 0; 2130 files checked, 0 skipped; four pre-existing out-of-lane warnings
git diff --check
# exit 0, no output
git diff --check b773ace6cd59196aaf11d3d3b40fd16cba20dc8d..e37d90692be6ca42db112c45b1862e1d862aae7e
# exit 0, no output
git diff --name-only b773ace6cd59196aaf11d3d3b40fd16cba20dc8d..e37d90692be6ca42db112c45b1862e1d862aae7e -- '*.json'
# exit 0, no output; python3 -m json.tool not applicable
```

The source-shape warnings are the same four pre-existing files outside this
lane: Settings Center test (583), audio-applet controller test (563), display
color-model test (539), and compositor CMake (500).

## Verdict

ACCEPT. P1-03 is closed by conforming behavior and a registered discriminating
regression, while the prior lineage, close-lifecycle, and hostile identity
closures remain green.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
