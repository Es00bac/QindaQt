# Maryam Mirzakhani — Color Settings route repair handoff (bounded repair of 944673b)

- Provider/model: Moonshot Kimi `kimi-code/k3` (reasoning high)
- Candidate: `252b2fd7d6d590178b33135af9d64123b1acf6cf`
- Tree: `fd7b8ecddc8f7f06c4f19e3aee19fcc77518429f`
- Exact base: `b971b43881fcef18980acec03c4e43e56ef9db2a`
- Branch: `worker/color-settings-route`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/color-settings-route`
- Verdict addressed: Maryna Viazovska (OpenAI Codex) REJECT of `944673b`
  at 0/1/1/0 (`/home/cabewse/work_SPaC3/builds/qindaqt/lanes/review-color-codex/verdict.md`)

## Repair summary

- P1 (fresh user cannot import a profile):
  `src/apps/settings/color/color_route_composition.cpp` now provisions the
  XDG `UserImported` root at composition startup. Only a directory the
  composition creates is tightened to mode 0700; an existing directory keeps
  the user's permissions and stays subject to the C1 writer's fail-closed
  validation. New registered row `qindaqt.settings-color-composition`
  (`tests/apps/settings/color/tst_color_route_composition.cpp`) redirects
  `XDG_DATA_HOME`/`XDG_DATA_DIRS` to a fresh build-local home and proves the
  mode-0700 EUID-owned root appears and a first import through the public
  C1 provider succeeds.
- P2 (no registered `desktop.virtual.stage-closure` row): the accepted,
  already-integrated guard is cherry-picked from main as ordinary commits —
  `eac04ccb` (`99b0619`, Guard DesktopVirtual stage closure) and `66478c40`
  (`91377acf`, Guard staged Settings route closure) — plus one additive edit
  in `tests/session/DesktopPackageTests.cmake` recording
  `QindaQt.SettingsApp.ColorBackend` as an embedded static module (it is
  linked into the Settings executable, exactly like PowerBackend), so the
  guard's `Main.qml`-derived closure accepts the Color route. The
  testing-harness page's route inventory and embedded-exception lists now
  name Color/ColorBackend; the color-settings page documents provisioning.

## Negative control (fails on the unrepaired tree)

With `src/apps/settings/color/color_route_composition.cpp` temporarily
restored to its `944673b` content and the test rebuilt, Debug
`ctest -R '^qindaqt\.settings-color-composition$'` fails both test functions
(root missing after composition; fresh import refused) — exit non-zero,
2 passed / 2 failed inside the row. The repair was then restored
(`git checkout HEAD --`) and the tree rebuilt; `git status` clean.

## Evidence (all run this round; isolated environment)

Environment for every ctest row:
`env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`,
profile-specific `HOME`/`XDG_DATA_HOME`/`XDG_CACHE_HOME`/`XDG_RUNTIME_DIR`
under the build root, offscreen software rendering. No nested-compositor,
host-bus, hardware, or network row was run.

- Debug configure (brief recipe, `qindaqt-665-initial-cache.cmake`): exit 0.
- Release configure (same recipe): exit 0.
- Debug focused build (`qindaqt-desktop-session-probe`, the new
  `qindaqt_color_route_composition_tests`, all 34 settings-selector test
  executables, the five applet module library/plugin pairs): exit 0.
  (An intermediate compile of the new test file failed on `qSetEnv`, absent
  in Qt 6.11; fixed in commit `252b2fd7` by using `qputenv`.)
- Release focused build (same target list): exit 0.
- Debug `ctest -R '^qindaqt\.settings-' --output-on-failure --no-tests=error`:
  exit 0, 55/55 (selector matches 55 rows, including the new composition row).
- Release same selector: exit 0, 55/55.
- Debug
  `ctest -R 'desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)'`:
  exit 0, 3/3.
- Release same selector: exit 0, 3/3.
- Debug `ctest -R '^desktop\.virtual\.stage-closure$'` (the exact P2 repro
  selector): exit 0, 1/1.
- Release `ctest -R '^desktop\.virtual\.stage-closure$'`: exit 0, 1/1.
- `./tools/validate-docs`: exit 0 (136 Markdown documents/navigation).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
  --strict --site-dir <build-root>/site`: exit 0.
- `./tools/check-source-shape`: exit 0 (warnings only for pre-existing
  review-threshold files).
- `git diff --check`: exit 0.
- Changed-JSON enumeration: none; no `python3 -m json.tool` inputs.

## Changed paths (base `b971b43`..candidate, sorted)

docs/wiki/apps/color-settings.md; docs/wiki/apps/settings-center.md;
docs/wiki/architecture/display-color-model.md;
docs/wiki/architecture/module-boundaries.md;
docs/wiki/development/testing-harness.md; docs/wiki/index.md;
docs/wiki/shell/applet-runtime.md; mkdocs.yml;
ops/team/messages/first-party-settings/1788453023-maryam-mirzakhani-color-settings-claim.md;
ops/team/messages/first-party-settings/1788456238-maryam-mirzakhani-color-settings-midpoint.md;
ops/team/messages/first-party-settings/1788458286-maryam-mirzakhani-handoff.md;
ops/team/workers/maryam-mirzakhani.md; src/CMakeLists.txt;
src/apps/settings/color/CMakeLists.txt;
src/apps/settings/color/color_route_composition.cpp;
src/apps/settings/color/color_route_composition.h;
src/apps/settings/color/color_settings_model.cpp;
src/apps/settings/color/color_settings_projection.cpp;
src/apps/settings/color/color_settings_projection.h;
src/apps/settings/color/include/qindaqt/apps/settings_color/color_settings_model.h;
src/apps/settings/color/qml/ColorImportSection.qml;
src/apps/settings/color/qml/ColorOutputSection.qml;
src/apps/settings/color/qml/ColorPage.qml;
src/apps/settings/color/qml/ColorProfileSection.qml;
src/apps/settings_center/CMakeLists.txt; src/apps/settings_center/Main.qml;
src/apps/settings_center/SettingsRouteHost.qml;
src/apps/settings_center/settings_route.cpp;
src/apps/settings_center/settings_route.h;
src/apps/settings_center/settings_route_registry.cpp;
tests/CMakeLists.txt; tests/apps/settings/bluetooth/CMakeLists.txt;
tests/apps/settings/color/CMakeLists.txt;
tests/apps/settings/color/check_boundary.cmake;
tests/apps/settings/color/check_boundary_negative.cmake;
tests/apps/settings/color/check_installed_route.cmake;
tests/apps/settings/color/color_navigation_assertions.h;
tests/apps/settings/color/color_settings_test_support.h;
tests/apps/settings/color/stub_color_settings_model.h;
tests/apps/settings/color/tst_color_navigation_page.cpp;
tests/apps/settings/color/tst_color_page.cpp;
tests/apps/settings/color/tst_color_route_composition.cpp;
tests/apps/settings/color/tst_color_settings_apply.cpp;
tests/apps/settings/color/tst_color_settings_model.cpp;
tests/apps/settings/customize/CMakeLists.txt;
tests/apps/settings_center/CMakeLists.txt;
tests/apps/settings_center/check_installed_routes.cmake;
tests/apps/settings_center/check_route_construction.cmake;
tests/apps/settings_center/tst_settings_navigation_controller.cpp;
tests/apps/settings_center/tst_settings_route_registry.cpp;
tests/session/CMakeLists.txt; tests/session/DesktopPackageTests.cmake;
tests/session/DesktopSessionRouteStaging.cmake;
tests/session/DesktopSessionTests.cmake;
tests/session/DesktopVirtualAppletModules.cmake;
tests/session/PanelVisibilityTests.cmake;
tests/session/desktop_session_stage_closure.py;
tests/session/test_desktop_session_stage_closure.py;
tests/session/test_desktop_session_stage_closure_unit.py

## Remaining bounded caveats

- The stage-closure guard code itself is the previously reviewed and
  main-integrated implementation, cherry-picked unchanged; the only new
  content is the additive ColorBackend embedded-module exemption.
- The system-KWin cache configure remains unusable for the KWin plugin
  target (6.6.6 vs the 6.6.5 EXACT pin), as the reviewer already recorded;
  the pinned 6.6.5 cache recipe was used for both profiles.
- This candidate claims no nested-session, rendered-output, compositor
  profile-application, colord, HDR/WCG, or hardware evidence; the color
  route still records assignment intents only.

## Requested next action

Independent exact review (Maryna Viazovska rechecks once), then manager
integration.
