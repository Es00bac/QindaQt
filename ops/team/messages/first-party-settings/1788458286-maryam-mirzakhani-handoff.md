# Color Settings route — candidate handoff (Maryam Mirzakhani)

- Time: 2026-09-03T11:58:06-06:00
- Worker: Maryam Mirzakhani (Moonshot Kimi `kimi-code/k3`), slug
  `maryam-mirzakhani`
- Outcome: first-class `qindaqt-settings --page color` route over the Display
  Color C1 boundary (QQ-006.05 Color page; QQ-005.07 Settings UI)

## Candidate

- Candidate commit: `944673bf45bf3d9f142e94940ad11aec9491d115`
- Candidate tree: `c8e9f65ab92db4a1ee1b34e421a4b7c200113cf5`
- Exact base: `b971b43881fcef18980acec03c4e43e56ef9db2a` (main tip at claim)
- Branch: `worker/color-settings-route`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/color-settings-route`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/color-settings-route`

## Changed paths (sorted)

```
docs/wiki/apps/color-settings.md
docs/wiki/apps/settings-center.md
docs/wiki/architecture/display-color-model.md
docs/wiki/architecture/module-boundaries.md
docs/wiki/development/testing-harness.md
docs/wiki/index.md
mkdocs.yml
src/CMakeLists.txt
src/apps/settings/color/CMakeLists.txt
src/apps/settings/color/color_route_composition.cpp
src/apps/settings/color/color_route_composition.h
src/apps/settings/color/color_settings_model.cpp
src/apps/settings/color/color_settings_projection.cpp
src/apps/settings/color/color_settings_projection.h
src/apps/settings/color/include/qindaqt/apps/settings_color/color_settings_model.h
src/apps/settings/color/qml/ColorImportSection.qml
src/apps/settings/color/qml/ColorOutputSection.qml
src/apps/settings/color/qml/ColorPage.qml
src/apps/settings/color/qml/ColorProfileSection.qml
src/apps/settings_center/CMakeLists.txt
src/apps/settings_center/Main.qml
src/apps/settings_center/SettingsRouteHost.qml
src/apps/settings_center/settings_route.cpp
src/apps/settings_center/settings_route.h
src/apps/settings_center/settings_route_registry.cpp
tests/CMakeLists.txt
tests/apps/settings/bluetooth/CMakeLists.txt
tests/apps/settings/color/CMakeLists.txt
tests/apps/settings/color/check_boundary.cmake
tests/apps/settings/color/check_boundary_negative.cmake
tests/apps/settings/color/check_installed_route.cmake
tests/apps/settings/color/color_navigation_assertions.h
tests/apps/settings/color/color_settings_test_support.h
tests/apps/settings/color/stub_color_settings_model.h
tests/apps/settings/color/tst_color_navigation_page.cpp
tests/apps/settings/color/tst_color_page.cpp
tests/apps/settings/color/tst_color_settings_apply.cpp
tests/apps/settings/color/tst_color_settings_model.cpp
tests/apps/settings/customize/CMakeLists.txt
tests/apps/settings_center/CMakeLists.txt
tests/apps/settings_center/check_installed_routes.cmake
tests/apps/settings_center/check_route_construction.cmake
tests/apps/settings_center/tst_settings_navigation_controller.cpp
tests/apps/settings_center/tst_settings_route_registry.cpp
tests/session/DesktopSessionRouteStaging.cmake
tests/session/DesktopSessionTests.cmake
```

`src/services/display_color_*` was read-only throughout. Coordination files
(this message, the claim/midpoint messages, and my worker record) follow in a
separate commit.

## Evidence (all commands actually run; isolated environment
`env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`
with `HOME`/`XDG_DATA_HOME` redirected under the build root and
`QT_QPA_PLATFORM=offscreen`)

Configure (exact recipe from the brief, both profiles): exit 0.

- Debug `ctest -R '^qindaqt\.settings-color-' --no-tests=error`: exit 0,
  **7/7 passed** (model, apply, page, navigation-page, boundary,
  boundary-poison, installed-route).
- Release same: exit 0, **7/7 passed**.
- Debug `ctest -R '^qindaqt\.settings-' --no-tests=error`: exit 0,
  **54/54 passed**.
- Release same: exit 0, **54/54 passed**.
- Debug `ctest -R 'desktop\.virtual\.(sandbox-unit|package-contract)'
  --no-tests=error`: exit 0, **2/2 passed**.
- Release same: exit 0, **2/2 passed**.
- `./tools/validate-docs`: exit 0.
- `mkdocs build --strict --site-dir <ROOT>/site` (docs venv): exit 0.
- `./tools/check-source-shape`: exit 0.
- `git diff --check`: exit 0 (clean).
- No JSON files changed, so `python3 -m json.tool` had no inputs.

Notes on evidence honesty:

- The settings selectors were rerun after the final tree state (the shared
  navigation-page test was reverted to avoid the 600-line cap; color host
  coverage lives in the color-owned
  `qindaqt.settings-color-navigation-page` row).
- `desktop.virtual.package-contract` required building the desktop stage
  targets first; both rows then passed in both profiles.
- No nested-compositor `tests/session` rows were run; no host bus, host
  display, real ICC directory, or hardware was contacted. The installed-route
  and route-construction rows run relocated binaries against poisoned buses
  and sandboxed XDG roots only.

## What the candidate delivers

- `src/apps/settings/color`: route model (Display1 inventory + C1 catalog +
  C1 assignment store), pure projection, engine-singleton backend
  composition (public Qt transports; XDG-derived injected roots), and four
  keyboard-accessible QML files with QST tokens and accessible
  names/roles/states, first-focus always on an enabled admitted control.
- Registration after `power`: enum value, registry entry, host mapping,
  Ctrl+0 shortcut, RPATH/import rows, installed component, DesktopVirtual
  staging, static ColorBackend linked into every in-process Main.qml host.
- Exact owner/epoch/revision admission; Settings1 draft/apply with
  conflict/uncertain surfaced and never replayed; stale retained-document
  truth; import through the bounded local file dialog (no portal); visible
  compositor-application nonclaim.
- Owning page `docs/wiki/apps/color-settings.md` plus additive rows in
  settings-center, module-boundaries, testing-harness, index, mkdocs.yml,
  and one accuracy note in display-color-model.

## Bounded caveats (deliberately not claimed)

- No compositor or display profile application, colord integration, HDR/WCG
  runtime behavior, profile-body interpretation, live AT-SPI, nested-session
  visuals, or physical hardware qualification.
- Import lineage for profiles imported before a route session is empty by C1
  contract (a rescan cannot recover digests); session imports carry the
  SHA-256 fingerprint into the draft record.
- Discovery scans only while the route is active; the catalog is local truth
  and never alone lifts the page out of unavailable.

## Requested next action

Independent exact review of commit `944673bf45bf3d9f142e94940ad11aec9491d115`,
then manager integration.
