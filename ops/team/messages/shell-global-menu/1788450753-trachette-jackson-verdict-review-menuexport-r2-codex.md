# Trachette Jackson — independent AppShell/global-menu repair recheck

- Persona: Trachette Jackson, independent shell/application reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Candidate SHA: `b773ace6cd59196aaf11d3d3b40fd16cba20dc8d`
- Tree SHA: `c014b8fb959be170fc436fc4239f70e5e108de0b`
- Parent SHA: `33ded824e9a1cde0657f21266e31473bbd78da29`
- Base SHA: `59353bf431b3a9d19f20e9db23617cb839fd1dda` (rejected product candidate and merge base)
- Implementer repair-work base: `e944b6d67ffeaf04e31bdbddaf1f22f8bf3d983d`
- Original lane base: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/app-menu-export-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex`

## Findings ledger

### P0

None.

### P1

#### P1-03 remains open — the replacement server still does not implement complete dbusmenu v4 semantics

`src/shell/global_menu/dbusmenu/src/dbusmenu_server.cpp:168-180` returns an empty `PropertyEntryList` when `GetGroupProperties` receives an empty ID list because it only iterates the caller-provided IDs. The standard contract says an empty ID list requests all menu items. This contradicts the candidate's “complete standard dbusmenu v4 server” claims in `docs/wiki/shell/global-menu.md:157-166`, `docs/wiki/adr/0065-compose-first-party-menu-export-through-appshell.md:72-79`, and the public header comment at `src/shell/global_menu/dbusmenu/include/qindaqt/shell/global_menu/dbusmenu/dbusmenu_server.h:16-20`.

The authoritative libdbusmenu interface XML states for `GetGroupProperties.ids`: “If the list is empty, all menu items should be sent.” See [Debian's source view of Canonical libdbusmenu's `dbus-menu.xml`](https://sources.debian.org/src/libdbusmenu/18.10.20180917~bzr492%2Brepack1-2/libdbusmenu-glib/dbus-menu.xml/).

Exact scratch reproduction (`repro.cpp` and build output are under the assigned build root):

```text
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/group-properties -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/group-properties/build -G Ninja
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/group-properties/build --parallel 1
/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/group-properties/build/group_properties_repro
# combined exit 1
publishedItems=2 emptyIdsResult=0
```

Expected: the empty-ID query returns properties for the two published non-root menu items. Observed: it returns zero entries. The registered test at `tests/shell/global_menu/dbusmenu/tst_dbusmenu_server.cpp:69-74` passes only one explicit action ID, so it cannot detect this standard request form even though its test name claims the grouped v4 surface. This leaves the substance of the prior P1-03 incomplete-server finding unresolved.

### P2

None.

### P3

None.

## Prior-finding recheck

- **P1-01 (rejected close withdrawal): closed.** `tests/app_shell/tst_application_menu_export.cpp:270-306` is a registered test with non-vacuous assertions: the rejected close must return false, leave the window visible, retain `published`, and keep unregister count at zero; the accepted follow-up must disable and unregister exactly once. The old candidate's pre-dispatch `stop()` at its `application_menu_export.cpp:399-403` necessarily fails those assertions. The original scratch reproducer rebuilt against this exact descendant now exits 0 with `closeAccepted=0 statusAfterRejectedClose=3 published=1`.
- **P1-02 (second lineage authority): closed.** `src/app_shell/menu_export/src/application_menu_export.cpp` now projects a default-lineage tree directly into the transport server, and the exact current-source scan finds none of `LocalExportLineage`, `QUuid::createUuid`, `ExportLineageSource`, `menu_exporter.h`, or `GlobalMenuExporter`. The registered source-policy assertions at `tests/app_shell/check_app_shell_source_policy.cmake:48-64` reject those spellings; the old-source scan found `LocalExportLineage`, two UUID issuers, and `lineage.advance()` at old lines 107-157, so the policy would fail on `59353bf`.
- **P1-03 (second AppShell server and absent methods): partially closed, still blocking.** The AppShell-private server files are deleted, AppShell consumes `DbusMenuServer`, and the rebuilt original reproducer reports `GetProperty=1 EventGroup=1 AboutToShowGroup=1`. The old header scan found only its `Q_CLASSINFO` and none of those three methods, so the new registered introspection assertions at `tests/app_shell/tst_application_menu_export.cpp:174-193` would fail on `59353bf`. The replacement's empty-ID behavior above nevertheless means it is not yet the complete v4 server the repair claims.
- **P2-01 (real File Manager hostile identity variants): closed.** The registered real-process row has matching, mismatched-PID, and mismatched-window-ID data rows; each negative row requires unavailable/empty shell state, zero activation, and a still-running child. Its direct run passes 5/5. Running the registered source sentinel over `59353bf` exits 1 on missing `QTest::newRow("mismatched-pid")`, proving that the new graph guard distinguishes the rejected candidate.
- Earlier positive coverage did not regress: registrar replacement/loss, accepted close, real File Manager matching activation, existing ownership/lineage tests, and the broader AppShell/File Manager/global-menu selector remain green in both build types.

## Commands and results

### Immutable tree checks

```text
git rev-parse HEAD
# b773ace6cd59196aaf11d3d3b40fd16cba20dc8d
git rev-parse HEAD^{tree}
# c014b8fb959be170fc436fc4239f70e5e108de0b
git rev-parse HEAD^
# 33ded824e9a1cde0657f21266e31473bbd78da29
git merge-base 59353bf431b3a9d19f20e9db23617cb839fd1dda HEAD
# 59353bf431b3a9d19f20e9db23617cb839fd1dda
git status --porcelain
# exit 0, no output before review
```

### Configure and focused builds

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
# exit 0
```

Both configurations retained the expected Qt `GuiPrivate` version-coupling warning and pre-existing dependency-root RPATH warnings; neither failed.

```text
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export qindaqt_global_menu_qmlplugin tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
# exit 0; focused incremental build completed all 123 remaining steps
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release --parallel 3 --target qindaqt-shell qindaqt-file-manager qindaqt_app_shellplugin qindaqt_app_shell_menu_export qindaqt_global_menu_qmlplugin tests/app_shell/all tests/apps/file_manager/all tests/shell/global_menu/all
# exit 0; focused incremental build completed all 123 remaining steps
```

### Focused and adjacent tests

I set `QT_FATAL_WARNINGS=1` for the complete selectors, which includes every QML row, in addition to the rows' registered environments.

```text
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
# exit 0; 46/46 passed; 0 failed; 14.50 s
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/release -R '^qindaqt\.(app-shell-|file-manager-|global-menu-)' --output-on-failure --no-tests=error
# exit 0; 46/46 passed; 0 failed; 12.73 s
```

Direct Debug evidence:

```text
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug/tests/app_shell/qindaqt_app_shell_menu_export_tests -txt
# exit 0; 7 passed, 0 failed
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_FATAL_WARNINGS=1 dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug/tests/shell/global_menu/runtime_composition/qindaqt_file_manager_menu_export_tests -txt
# exit 0; 5 passed, 0 failed, including matching/PID-mismatch/window-ID-mismatch
/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug/tests/shell/global_menu/dbusmenu/qindaqt_global_menu_dbusmenu_server_tests -txt
# exit 0; 4 passed, 0 failed
```

Original combined P1-01/P1-03 scratch reproduction rebuilt against the descendant:

```text
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close/build -G Ninja
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close/build --parallel 1
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 dbus-run-session -- /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/ignored-close/build/ignored_close_repro
# exit 0
version4Methods GetProperty=1 EventGroup=1 AboutToShowGroup=1
closeAccepted=0 statusAfterRejectedClose=3 published=1
```

Old-candidate P2-01 graph-control check:

```text
git show 59353bf431b3a9d19f20e9db23617cb839fd1dda:tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp | cmake -DSOURCE_FILE=/dev/stdin -P tests/shell/global_menu/runtime_composition/check_file_manager_identity_variants.cmake
# exit 1: missing QTest::newRow("mismatched-pid")
```

No `tests/session`, host session/system service, hardware, uinput, network, or nested-compositor row was run. All D-Bus runtime tests used their private `dbus-run-session`; the ambient session bus was removed and the system bus was fenced to `/nonexistent`.

### Static and documentation gates

```text
./tools/validate-docs
# exit 0; 131 Markdown documents and mkdocs.yml navigation validated
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/site
# exit 0; documentation built in 1.55 s
./tools/check-source-shape
# exit 0; 2130 files checked, 0 skipped; four pre-existing warnings outside this lane
git diff --check
# exit 0, no output
git diff --check 59353bf431b3a9d19f20e9db23617cb839fd1dda..b773ace6cd59196aaf11d3d3b40fd16cba20dc8d
# exit 0, no output
git diff --name-only 59353bf431b3a9d19f20e9db23617cb839fd1dda..b773ace6cd59196aaf11d3d3b40fd16cba20dc8d -- '*.json'
# exit 0, no output; no changed JSON, so python3 -m json.tool was not applicable
```

The four source-shape warnings are pre-existing and outside the candidate: Settings Center test (583), audio-applet controller test (563), display-color-model test (539), and compositor CMake (500). `tests/session/DesktopSessionTests.cmake` remains 499 and was not executed.

## Verdict

REJECT. P1-01, P1-02, and P2-01 are repaired with meaningful regression evidence, and the AppShell-private server/absent-method portions of P1-03 are repaired. P1-03 remains blocking because the replacement advertised as a complete dbusmenu v4 server violates the standard empty-ID `GetGroupProperties` request and the only new grouped-call test omits that form.

VERDICT REJECT P0/P1/P2/P3=0/1/0/0
