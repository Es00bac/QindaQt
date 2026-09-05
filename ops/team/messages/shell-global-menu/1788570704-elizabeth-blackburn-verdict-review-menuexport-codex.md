# Elizabeth Blackburn — independent first-party exact-candidate review

- Persona: **Elizabeth Blackburn**, independent first-party reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Exact candidate SHA: `e8e5170beb9849752341d1fec1ad47ffa5e6ca1e`
- Tree SHA: `3c79b361dfbcdd0e94a6d5f193d0bec1179ca2bb`
- Parent SHA: `86ad7c3854a5e35efb0a3b3e65444c432448fa0e`
- Base SHA: `86ad7c3854a5e35efb0a3b3e65444c432448fa0e`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export-codex-review`

## Findings ledger

### P0

None.

### P1

#### P1-01 — Native surface recreation leaves the exported menu bound to the old window identity

`ApplicationMenuExport::eventFilter()` handles only `QEvent::Close` at `src/app_shell/menu_export/src/application_menu_export.cpp:335-350`. It does not handle the `QEvent::PlatformSurface` destruction/recreation lifecycle. The candidate routes Terminal and Text Editor through this implementation via `composeFirstPartyMenuExport()` at `src/app_shell/menu_export/src/first_party_composition.cpp:69` (callers: `src/apps/terminal/main.cpp:137` and `src/apps/text_editor/main.cpp:210`).

Reproduction built outside the worktree against the candidate's real Debug AppShell and registrar libraries. It creates a real `QWindow`, publishes identity 71, destroys and recreates its native surface without destroying the `QWindow` object, and makes the identity publisher return 72 if asked to republish:

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/window-recreation \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/window-recreation/build \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/window-recreation/build --parallel 1
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_QPA_PLATFORM=offscreen QT_FATAL_WARNINGS=1 \
  dbus-run-session -- \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/window-recreation/build/window_recreation_repro
```

Results: configure exit 0; build exit 0; reproduction exit 0, where exit 0 deliberately means the stale-identity predicate was observed. Exact output:

```text
surfaceCreated=2 surfaceDestroyed=1 publishCount=1 withdrawCount=0 oldId71=registered newId72=absent status=3
expected after surface recreation: publishCount=2, oldId71=absent, newId72=registered
REPRODUCED stale window identity
```

Observed: despite a genuine surface destroy/create cycle, the export remains `Published`, registration 71 remains live, identity 72 is absent, and neither withdraw nor republish occurs. Expected: withdraw identity 71 during native-surface teardown and publish/register the recreated surface as identity 72. Under the shell's exact window-identity join, a recreated Terminal or Text Editor surface therefore has no matching menu registration until another unrelated recovery event (such as registrar-owner replacement or process restart). This violates the funded lane's explicit window-recreation and exact-identity lifecycle requirement.

The focused tests contain registrar-owner-replacement and close-event coverage but no `PlatformSurface` destruction/recreation negative control; `rg -n 'PlatformSurface|SurfaceAboutToBeDestroyed' tests/app_shell tests/apps/terminal tests/apps/text_editor` produced no matches.

### P2

None.

### P3

None.

## Commands and results

### Candidate identity and review surface

```sh
git rev-parse HEAD
git rev-parse 'HEAD^{tree}'
git rev-parse HEAD^
git rev-parse 86ad7c38
git status --porcelain=v1
```

Exit 0. The values matched the candidate, tree, parent, and base recorded above; status output was empty before review and remained empty at final verification. `git diff --stat 86ad7c3854a5e35efb0a3b3e65444c432448fa0e e8e5170beb9849752341d1fec1ad47ffa5e6ca1e` reported 23 files, 1,481 insertions, and 113 deletions.

### Configure

Debug:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Release used the identical command with the build directory changed to `release` and `-DCMAKE_BUILD_TYPE=Release`. Both exited 0.

The assigned build directories initially contained caches for another source worktree. To preserve them while ensuring an exact-source configure, they were moved within the assigned build root to `prior-app-menu-debug` and `prior-app-menu-release`; the prescribed directories were then configured fresh.

### Build

For both Debug and Release:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/<config> --parallel 3 --target \
  src/app_shell/menu_export/all src/apps/file_manager/all src/apps/terminal/all \
  src/apps/text_editor/all tests/app_shell/all tests/apps/file_manager/all \
  tests/apps/terminal/all tests/apps/text_editor/all tests/shell/global_menu/all
```

Both exited 0 after 663/663 steps.

The first exact-selector execution exposed that this focused target closure did not produce the installed-package QML plugin or production shell artifact: both configurations exited 8 with 82/89 passing and seven executable/plugin-not-built failures (`qindaqt.global-menu-installed-package` and tests 177-182). This was a reviewer build-closure issue, not a product-test failure. The missing adjacent targets were then built exactly with:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/<config> \
  --parallel 3 --target qindaqt_global_menu_qmlplugin qindaqt-shell
```

Both Debug and Release exited 0 after 636/636 steps. No nested compositor, windowed host session, host service, hardware, uinput, or network row was run.

### Focused and adjacent tests

Test discovery in each configuration:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/<config> \
  -N -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)'
```

Exit 0; `Total Tests: 89` in Debug and Release.

Final execution in each configuration:

```sh
env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/<config> \
  -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0; 89/89 passed; 0 failed; 39.03 seconds.
- Release: exit 0; 89/89 passed; 0 failed; 36.17 seconds.

This includes the existing Terminal PTY bridge, session, process-group, tabs, search/link, and adapter rows; the Text Editor document, controller, large/multiple-document, restore, find/replace, window, and CLI rows; and the candidate's private-bus identity, absent-registrar, and hostile-registrar rows.

### Static, documentation, and JSON gates

```sh
./tools/validate-docs
```

Exit 0; validated 144 Markdown documents and `mkdocs.yml` navigation.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/site
```

Exit 0; strict site build completed in 2.20 seconds.

```sh
./tools/check-source-shape
```

Exit 0; checked 2,552 source files with no errors. It printed only pre-existing warnings on untouched files.

```sh
git diff --check 86ad7c3854a5e35efb0a3b3e65444c432448fa0e e8e5170beb9849752341d1fec1ad47ffa5e6ca1e
```

Exit 0 with no output.

```sh
git diff --name-only 86ad7c3854a5e35efb0a3b3e65444c432448fa0e \
  e8e5170beb9849752341d1fec1ad47ffa5e6ca1e -- '*.json'
```

Exit 0 with no output. There are no changed JSON files, so no `python3 -m json.tool` invocation applies.

## Verdict

The exact candidate passes its focused and adjacent automated suites and static gates, but the independently reproduced P1 stale-identity defect violates the required window-recreation lifecycle contract. Acceptance is blocked.

VERDICT REJECT P0/P1/P2/P3=0/1/0/0
