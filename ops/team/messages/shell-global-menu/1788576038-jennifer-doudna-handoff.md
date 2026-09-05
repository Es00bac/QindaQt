# Jennifer Doudna — fail-closed registrar classification handoff

- Time: 2026-09-04T20:40:38-06:00
- Exact candidate commit: `bfe6009950187901e4f42902aa211ab3378419aa`
- Candidate tree: `d991e598b48375caeadd518d221ba1364b838ed5`
- Exact base: `e01fcd16a07fd6a07251cb111b3afe4e55bf41bc`
- Branch: `worker/first-party-menu-export`
- Requested next action: independent exact review then manager integration.

## Changed paths

- `docs/wiki/shell/global-menu.md`
- `src/app_shell/menu_export/src/application_menu_export.cpp`
- `tests/app_shell/tst_application_menu_export_inflight.cpp`

## Outcome

`RegisterWindow` is confirmed only by a `ReplyMessage` with an exactly empty
signature. Registrar method errors, including `AccessDenied` and
`UnknownMethod`, are definitive refusals and clear the attempted-id debt.
Local timeout/`NoReply`, disconnect/owner-lifecycle errors, and non-empty or
unexpected success replies remain uncertain: the attempted id and exact owner
stay owed until one withdrawal sends `UnregisterWindow` exactly once. Serial
fencing keeps all late replies inert. The global-menu page now audits send,
reply, error, timeout, surface destroy/create, rejected/accepted close,
stop/quit/window destruction, and registrar owner loss/restart against named
executable rows.

## Evidence

Configuration used the prescribed command for both build trees:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/first-party-menu-export/<debug|release> -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=<Debug|Release> -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Debug exit 0; Release exit 0. Both used system KWin 6.6.6, Qt 6.11.1, and KF6
6.27 through the prescribed cache. The only configure warning was the expected
Qt GuiPrivate exact-version coupling accepted by ADR-0068.

The exact target list was built in both configurations:

```sh
cmake --build <configuration-root> --parallel 3 --target \
  qindaqt_app_shell_menu_export_inflight_tests \
  qindaqt_app_shell_menu_export_surface_tests \
  qindaqt_app_shell_menu_export_tests src/app_shell/menu_export/all \
  src/apps/file_manager/all src/apps/terminal/all src/apps/text_editor/all \
  tests/apps/file_manager/all tests/apps/terminal/all \
  tests/apps/text_editor/all tests/shell/global_menu/all \
  qindaqt_global_menu_qmlplugin qindaqt-shell
```

Debug exit 0; Release exit 0. This built the exporter, all three first-party
consumers, their focused/adjacent tests, the global-menu QML plugin, and the
production shell target with warnings as errors.

Direct repaired private-bus subtests:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent dbus-run-session -- \
  env -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen \
  QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/first-party-menu-export/debug/tests/app_shell/qindaqt_app_shell_menu_export_inflight_tests -v1
```

Exit 0; 7 passed, 0 failed, 0 skipped. The three registered AppShell
menu-export CTest rows also exited 0 with 3/3 passed in Debug.

The exact new test source was compiled against the immutable `9becfb1e` source
and libraries after confirming that review worktree's HEAD. Under the same
private-bus/offscreen isolation:

- `acceptedButNeverRepliedTimeoutIsCompensatedExactlyOnce`: exit 1 as the
  intended negative control; 2 passed / 1 failed, with old actual failure code
  `registrar-registration-failed` instead of the required uncertain state.
- `malformedSuccessPayloadRemainsUncertainUntilWithdrawal`: exit 1 as the
  intended negative control; 2 passed / 1 failed, with old actual failure code
  empty because the malformed reply was incorrectly published.

The reviewer's original standalone predicates were also built under this
lane's assigned build root. Against exact `9becfb1e`, timeout and malformed
predicates each exited 0 and printed `REPRODUCED`. Relinked against this
candidate's Debug library, each predicate exited 1 and printed
`NOT REPRODUCED`; timeout withdrawal recorded one unregister and cleared the
registrar-held id, while malformed success remained unpublished with
`registrar-registration-uncertain`.

Required adjacent discovery and execution for both configurations:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <configuration-root> -N \
  -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)'

env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir <configuration-root> \
  -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)' \
  --output-on-failure --no-tests=error
```

- Debug discovery: exit 0, 91 tests. Execution: exit 0, 91/91 passed, 0
  failed, 47.59 seconds.
- Release discovery: exit 0, 91 tests. Execution: exit 0, 91/91 passed, 0
  failed, 42.80 seconds.

Static/documentation gates:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/first-party-menu-export/site
./tools/check-source-shape
git diff --check
```

All exited 0. `validate-docs` checked 144 Markdown documents plus navigation;
strict MkDocs completed in 1.86 seconds; source-shape checked 2,554 files with
no skipped files and only pre-existing warnings on untouched paths. The changed
production source has 478 non-blank lines and the changed test has 372. No JSON
changed, so no `python3 -m json.tool` command applied.

## Bounded caveats

- Evidence is offscreen/private-bus only. It does not claim a host login
  session, nested compositor, native live Wayland/XWayland session, hardware,
  uinput, foreign-toolkit exporter, or network qualification.
- A disconnected bus can make delivery of the one compensating call
  impossible; the exporter still retains the attempt until withdrawal,
  targets the exact attempted owner once, and never reports it as published.
- The existing Qt 6.11 private KDE appmenu identity hook and its exact-version
  rebuild requirement are unchanged.
