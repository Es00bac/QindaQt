# Mary Cartwright — AppShell menu-export repair handoff

## Candidate

- Candidate commit: `b773ace6cd59196aaf11d3d3b40fd16cba20dc8d`.
- Candidate tree: `c014b8fb959be170fc436fc4239f70e5e108de0b`.
- Exact repair base: `e944b6d67ffeaf04e31bdbddaf1f22f8bf3d983d`.
- Rejected product candidate: `59353bf431b3a9d19f20e9db23617cb839fd1dda`.
- Original lane base: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f`.
- Branch: `worker/app-menu-export`.

## Finding closure

- **P1-01 — rejected close withdrawal:** closing commit `33ded824e9a1cde0657f21266e31473bbd78da29`; `ApplicationMenuExportTest::rejectedCloseKeepsLiveMenuPublished` carries the P1-01 `AGENT-NOTE:` and proves a rejected close retains the live registration, while the accepted follow-up closes and withdraws once.
- **P1-02 — second lineage authority:** closing commit `33ded824e9a1cde0657f21266e31473bbd78da29`; `qindaqt.app-shell-source-policy` carries the P1-02 `AGENT-NOTE:` and rejects local UUID/owner/epoch/revision sources plus exporter imports. AppShell now projects lineage-free content; the authenticated shell selector remains the sole lineage issuer.
- **P1-03 — duplicate incomplete server:** closing commit `33ded824e9a1cde0657f21266e31473bbd78da29`; `qindaqt.app-shell-source-policy` rejects an AppShell-local dbusmenu interface, `qindaqt.app-shell-menu-export-private-bus` introspects the three previously absent methods, and `qindaqt.global-menu-dbusmenu-server` exercises the complete filtered/grouped v4 surface and last-known-good malformed-snapshot behavior. Each regression proof names P1-03 in an `AGENT-NOTE:`.
- **P2-01 — missing real File Manager mismatch variants:** closing commit `b773ace6cd59196aaf11d3d3b40fd16cba20dc8d`; `qindaqt.file-manager-global-menu-shell-private-bus` runs matching, PID-mismatched, and window-ID-mismatched real child processes, and `qindaqt.file-manager-global-menu-identity-variants-source-policy` carries the P2-01 `AGENT-NOTE:` so omission itself fails the registered graph.

## Changed paths

- `docs/wiki/adr/0056-adopt-standard-appmenu-dbusmenu-transports.md`
- `docs/wiki/adr/0065-compose-first-party-menu-export-through-appshell.md`
- `docs/wiki/apps/application-shell.md`
- `docs/wiki/apps/file-manager.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/global-menu.md`
- `src/app_shell/menu_export/CMakeLists.txt`
- `src/app_shell/menu_export/src/application_menu_export.cpp`
- `src/app_shell/menu_export/src/dbusmenu_export_object.cpp` (deleted)
- `src/app_shell/menu_export/src/dbusmenu_export_object_p.h` (deleted)
- `src/shell/global_menu/dbusmenu/CMakeLists.txt`
- `src/shell/global_menu/dbusmenu/include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_server.h`
- `src/shell/global_menu/dbusmenu/include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_wire.h`
- `src/shell/global_menu/dbusmenu/src/dbusmenu_server.cpp`
- `src/shell/global_menu/dbusmenu/src/dbusmenu_wire.cpp`
- `tests/app_shell/check_app_shell_source_policy.cmake`
- `tests/app_shell/tst_application_menu_export.cpp`
- `tests/shell/global_menu/dbusmenu/CMakeLists.txt`
- `tests/shell/global_menu/dbusmenu/tst_dbusmenu_server.cpp`
- `tests/shell/global_menu/runtime_composition/CMakeLists.txt`
- `tests/shell/global_menu/runtime_composition/check_file_manager_identity_variants.cmake`
- `tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp`

## Reproduction evidence against `59353bf`

All commands ran from `/home/cabewse/work_SPaC3/container-wm-workers/app-menu-export` unless the executable path says otherwise.

- Reviewer ignored-close reproduction, exit 1:

  ```sh
  env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close/build/ignored_close_repro
  ```

  It reported `version4Methods GetProperty=0 EventGroup=0 AboutToShowGroup=0` and `closeAccepted=0 statusAfterRejectedClose=0 published=0`.

- `git show 59353bf:src/app_shell/menu_export/src/application_menu_export.cpp | rg -n 'LocalExportLineage|createUuid|advance|QEvent::Close|stop\(\)'`: exit 0 and located the local lineage issuer and pre-dispatch close teardown.
- `git show 59353bf:tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp | rg -n 'mismatch|processId|publishIdentity|private Q_SLOTS'`: exit 0 and showed one non-data-driven real-process row without either mismatch variant.
- The repaired registered P2-01 sentinel run over the rejected source, exit 1:

  ```sh
  git show 59353bf431b3a9d19f20e9db23617cb839fd1dda:tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp | cmake -DSOURCE_FILE=/dev/stdin -P tests/shell/global_menu/runtime_composition/check_file_manager_identity_variants.cmake
  ```

  It failed on missing `QTest::newRow("mismatched-pid")` before reaching the independently required window-ID row.

## Acceptance evidence

- Debug configure, exit 0:

  ```sh
  cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
  ```

- Release configure, exit 0: the same command with `-B /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/release -DCMAKE_BUILD_TYPE=Release`.
- Focused strict build in both Debug and Release, exit 0 for each:

  ```sh
  cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/<profile> --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export qindaqt_global_menu_qmlplugin tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
  ```

- Debug selector, exit 0, 46/46 passed in 14.39 seconds:

  ```sh
  env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/debug -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
  ```

- Release selector, exit 0, 46/46 passed in 12.59 seconds: the same command with `--test-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/release`. Registered QML rows retain `QT_FATAL_WARNINGS=1`; product rows use private buses/injected fakes and no host desktop resources.
- Direct private-bus AppShell executable: exit 0, 7/7 passed, including the rejected-close and v4-introspection cases.
- Direct real File Manager composition executable: exit 0, 5/5 passed, including both hostile identity variants.
- Direct dbusmenu server executable: exit 0, 4/4 passed.
- `./tools/validate-docs`: exit 0, 131 Markdown documents and `mkdocs.yml` navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/app-menu-export/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2130 files checked. Four warnings are pre-existing paths outside this lane: Settings Center test (583), audio-applet controller test (563), display-color-model test (539), and compositor CMake (500). `tests/session/DesktopSessionTests.cmake` is 499 on this tree and did not warn.
- `git diff --check`: exit 0 before both product commits and after the final source-policy addition.
- No JSON changed, so no JSON parser gate applied.

## Bounded caveats

- This candidate proves standard dbusmenu traffic, complete v4 method presence/semantics, exact X window identity through injected facts, registrar replacement/loss, accepted/rejected close behavior, exactly-once activation, and matching/mismatched real File Manager process joins on private buses.
- It deliberately does not claim host-session, nested-compositor, native Wayland protocol round-trip, physical hardware/display, GTK exporter, Text Editor, or Terminal evidence.
- Native Wayland identity publication remains confined to Qt 6.11's private KDE appmenu hook as documented by ADR-0065; the production shell still authenticates announced service/path and PID.
- Configure retained the expected Qt GuiPrivate version-coupling and pre-existing dependency RPATH warnings; strict warnings-as-errors focused builds passed.

## Requested next action

Trachette Jackson (OpenAI Codex): independent exact review of `b773ace6cd59196aaf11d3d3b40fd16cba20dc8d`, then manager integration.
