# Customize Settings canvas repaired candidate handoff

- Worker: **Lynn Conway** (`lynn-conway`)
- Posted: 2026-09-02T23:16:56-06:00 (unix 1788412616)
- Exact repaired candidate: `5a411b52da08f09a58be351daa47b60b1ef51a6c`
- Candidate tree: `7ac888684b5b4f302073a48060e019cb753f1bef`
- Rejected ancestor: `a6ea864ec2162dd5c1a8fba9f2d8f1f2f09a321f`
- Exact original base: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a`
- Branch: `worker/customize-settings-canvas`

## Repaired outcome

The candidate qualifies the Controls overlay reference and proves centered
discard-dialog geometry under fatal QML warnings. The Settings application now
routes title-bar and platform-Quit close through the existing dirty-departure
decision; Cancel keeps both window and draft. The injected route model is shared
by the wide and compact hosts, and a pending navigation decision reconstructs
its modal prompt after crossing the responsive threshold.

All three P3 findings are also closed: the constant profile projection no
longer carries stale selected truth, every owned C++/QML source participates in
the boundary scan with one explicit Settings1 composition exception, and the
responsive race has its own warning-fatal regression.

## Changed product paths (sorted)

- `docs/wiki/apps/customize-settings.md`
- `docs/wiki/apps/settings-center.md`
- `docs/wiki/development/testing-harness.md`
- `src/apps/settings/customize/customize_settings_projection.cpp`
- `src/apps/settings/customize/qml/CustomizeActionBar.qml`
- `src/apps/settings/customize/qml/CustomizeRoute.qml`
- `src/apps/settings_center/Main.qml`
- `src/apps/settings_center/SettingsRouteHost.qml`
- `tests/apps/settings/customize/CMakeLists.txt`
- `tests/apps/settings/customize/check_boundary.cmake`
- `tests/apps/settings/customize/tst_customize_page.cpp`
- `tests/apps/settings/customize/tst_customize_settings_model.cpp`
- `tests/apps/settings/customize/tst_customize_window_lifecycle.cpp`

## Acceptance evidence

The existing Debug and Release configurations were reused. Their CMake
regeneration retained the lane's strict-warnings and private KWin 6.6.5 cache.

Focused Debug build, exit 0:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/debug --parallel 3 --target qindaqt_settings_customize_model_tests qindaqt_settings_customize_page_tests qindaqt_settings_customize_window_lifecycle_tests qindaqt-settings qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test qindaqt_panel_editing_tests qindaqt_applet_editing_tests qindaqt_preview_history_tests qindaqt_coordinator_lease_tests qindaqt_editor_query_tests qindaqt_customize_editor_intent_tests qindaqt_customize_editor_gesture_tests qindaqt_customize_editor_session_tests qindaqt_customize_editor_dirty_state_tests qindaqt_customize_editor_persistence_tests qindaqt_customize_editor_accessibility_tests
```

Focused Release build with the same 18 targets, exit 0:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/release --parallel 3 --target qindaqt_settings_customize_model_tests qindaqt_settings_customize_page_tests qindaqt_settings_customize_window_lifecycle_tests qindaqt-settings qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test qindaqt_panel_editing_tests qindaqt_applet_editing_tests qindaqt_preview_history_tests qindaqt_coordinator_lease_tests qindaqt_editor_query_tests qindaqt_customize_editor_intent_tests qindaqt_customize_editor_gesture_tests qindaqt_customize_editor_session_tests qindaqt_customize_editor_dirty_state_tests qindaqt_customize_editor_persistence_tests qindaqt_customize_editor_accessibility_tests
```

Exact Customize selector, exit 0 and 6/6 passed in each profile:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/debug -R '^qindaqt\.settings-customize-' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/release -R '^qindaqt\.settings-customize-' --output-on-failure --no-tests=error
```

Complete customization/editor selector, exit 0 and 17/17 passed in each
profile:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/debug -R 'customiz' --output-on-failure --no-tests=error
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/release -R 'customiz' --output-on-failure --no-tests=error
```

Settings Center selector, exit 0 and 9/9 passed in each profile:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/debug --output-on-failure --no-tests=error -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$'
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/release --output-on-failure --no-tests=error -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$'
```

Additional Settings navigation warning-fatal probe, exit 0 and 1/1 passed in
each profile:

```sh
env QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/debug -R '^qindaqt\.settings-navigation-page$' --output-on-failure --no-tests=error
env QT_FATAL_WARNINGS=1 ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/release -R '^qindaqt\.settings-navigation-page$' --output-on-failure --no-tests=error
```

Static gates, all exit 0:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/site
./tools/check-source-shape
git diff --check
git diff --check HEAD^ HEAD
```

`validate-docs` checked 117 Markdown documents. Source shape checked 1,787
files and reported only the two pre-existing decomposition-review warnings
outside candidate paths. No JSON changed, so no `json.tool` command applies.

During the repair loop, the first focused lifecycle run correctly failed on a
missing route-level Cancel signal, and the next build correctly failed the
strict compiler on Qt 6.11's deprecated `QQmlPropertyMap` constructor. Both
were repaired before the complete evidence above; no failing output belongs to
the candidate state.

## Bounded caveats

- No session/nested-compositor, host D-Bus, hardware, uinput, network, live
  AT-SPI, or physical input row was run or is claimed.
- Live shell preview/application, the always-hidden reveal affordance, and the
  nested rendered matrix remain the original explicit downstream boundaries.
- No JSON or persisted schema changed.

Requested next action: **independent exact review of
`5a411b52da08f09a58be351daa47b60b1ef51a6c`, then manager integration**.
