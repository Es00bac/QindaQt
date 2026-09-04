# Maryam Mirzakhani — Color Settings route, bounded repair of `252b2fd` (round 2)

- Exact candidate SHA: `85c8e8c9e54db9c820f8f1934996292219a43dd5`
- Tree SHA: `eed3fd919ffc15ce87860d10c332cf8769639988`
- Exact base SHA: `b971b43881fcef18980acec03c4e43e56ef9db2a` (parent chain: candidate `85c8e8c9` → handoff record `0862d2c4` → rejected `252b2fd7`)
- Branch: `worker/color-settings-route`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/color-settings-route`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/color-settings-route`
- Requested next action: independent exact review (Maryna Viazovska rechecks once), then manager integration.

## What the repair changes

Maryna Viazovska's single P1 on `252b2fd` (0/1/0/0): the composition emitted
`qWarning` when the Settings1 client could not start on a disconnected session
bus, and `Main.qml` evaluates `ColorRouteComposition.model` eagerly, so three
warning-fatal host rows aborted with `DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent`
(Debug and Release 52/55).

- `src/apps/settings/color/color_route_composition.cpp`: the Settings1 start
  failure is now logged at info level on the
  `qindaqt.settings.color.composition` category (`qCInfo`, explicitly
  `QtInfoMsg` default). An unreachable session bus is an expected degraded
  state; the model already presents the assignment document as unavailable
  through its normal availability truth (fail-closed; no behavior claim beyond
  the existing degraded path). The user-root provisioning `qWarning`s are
  unchanged — they report genuine filesystem provisioning failures, not bus
  state. The smallest option was chosen: no `Main.qml` edit was needed.
- CTest environments pin both buses to `unix:path=/nonexistent`:
  `qindaqt.settings-navigation-page`, `qindaqt.settings-customize-window-lifecycle`,
  `qindaqt.settings-bluetooth-window-close` (session address added; system was
  already pinned), and all four Color rows that lacked the session pin
  (`color-model`, `color-apply`, `color-page`, `color-navigation-page`;
  `color-composition` already pinned both). These rows therefore fail on
  `252b2fd` and pass after the repair on any host.
- Docs: `docs/wiki/apps/color-settings.md` records the degraded-state choice
  and the executable pinning; `docs/wiki/development/testing-harness.md`
  Color section shows the pinned selector and names the three pinned host rows.

## Changed paths (sorted)

- docs/wiki/apps/color-settings.md
- docs/wiki/development/testing-harness.md
- src/apps/settings/color/color_route_composition.cpp
- tests/apps/settings/bluetooth/CMakeLists.txt
- tests/apps/settings/color/CMakeLists.txt
- tests/apps/settings/customize/CMakeLists.txt
- tests/apps/settings_center/CMakeLists.txt

## Evidence (all commands actually run)

Negative control (unrepaired tree, before the edit):

```sh
env -u DISPLAY -u WAYLAND_DISPLAY DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent HOME=<build>/xdg-ctl-debug/home \
  XDG_CONFIG_HOME=<build>/xdg-ctl-debug/config XDG_DATA_HOME=<build>/xdg-ctl-debug/data \
  XDG_CACHE_HOME=<build>/xdg-ctl-debug/cache XDG_RUNTIME_DIR=<build>/xdg-ctl-debug/runtime \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  <build>/debug/tests/apps/settings_center/qindaqt_settings_navigation_page_test -nocrashhandler
```

Exit 134 (SIGABRT) after `QWARN ... color Settings1 client unavailable: settings
session D-Bus is not connected` — the exact verdict reproduction. After the
repair the same command exits 0 (`Totals: 6 passed, 0 failed`), no QWARN.

Builds (both exit 0, strict warnings on):

```sh
cmake --build <build>/debug   --parallel 3 --target qindaqt-settings \
  qindaqt_settings_navigation_page_test qindaqt_settings_customize_window_lifecycle_tests \
  qindaqt_bluetooth_window_close_tests qindaqt_color_settings_model_tests \
  qindaqt_color_settings_apply_tests qindaqt_color_route_composition_tests \
  qindaqt_color_page_tests qindaqt_color_navigation_page_tests
cmake --build <build>/release --parallel 3 --target <same>
```

Selectors, run once per profile with `env -u DISPLAY -u WAYLAND_DISPLAY`,
both bus addresses pinned to nonexistent sockets, and HOME/XDG redirected
under `<build>/xdg-unreachable-<profile>/`:

```sh
ctest --test-dir <build>/<profile> -R '^qindaqt\.settings-' \
  --output-on-failure --no-tests=error --parallel 3
```

- Debug: exit 0, 55/55 passed.
- Release: exit 0, 55/55 passed.

```sh
ctest --test-dir <build>/<profile> \
  -R '^desktop\.virtual\.(sandbox-unit|package-contract|stage-closure)$' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 3/3.
- Release: exit 0, 3/3.

Static gates (from the worktree root):

- `./tools/validate-docs` — exit 0 (136 Markdown documents + mkdocs.yml).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <build>/site` — exit 0 (1.88 s).
- `./tools/check-source-shape` — exit 0 (2324 files, skipped 0; only pre-existing decomposition-review warnings).
- `git diff --check` — exit 0.
- JSON: no JSON file changed; nothing to validate.

## Bounded caveats

- This candidate claims only the P1 repair: no-warning degraded bus handling
  plus executable bus pinning. All other behavior of `252b2fd` is unchanged.
- No nested compositor, host D-Bus service, hardware, uinput, or network row
  was run. The KWin-plugin/current-main compatibility gate remains the
  manager/integration-assistant check.
- The provisioning `qWarning` paths (genuine filesystem failures) remain
  warnings by design; they are not reachable in the pinned selector rows
  because the isolated XDG home is writable.
