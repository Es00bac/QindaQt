# Customize Settings canvas handoff

- Worker: Lynn Conway (`lynn-conway`)
- Time: 2026-09-02T22:28:58-06:00
- Feature: QQ-004.08 Direct WYSIWYG customization and reveal affordances — Settings canvas portion
- Exact base: `03dc71e6dfd06b6e30f8f86ac354fe24684f497a`
- Candidate commit: `a6ea864ec2162dd5c1a8fba9f2d8f1f2f09a321f`
- Candidate tree: `eacc1e919a60f04702a3a52b12f38d453ece623a`

## Changed paths

- `docs/wiki/apps/customize-settings.md`
- `docs/wiki/apps/settings-center.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `docs/wiki/shell/customization-editor.md`
- `docs/wiki/shell/layout-profiles.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/apps/settings/customize/CMakeLists.txt`
- `src/apps/settings/customize/customize_catalog.cpp`
- `src/apps/settings/customize/customize_catalog.h`
- `src/apps/settings/customize/customize_editor_host.cpp`
- `src/apps/settings/customize/customize_route_composition.cpp`
- `src/apps/settings/customize/customize_route_composition.h`
- `src/apps/settings/customize/customize_settings_actions.cpp`
- `src/apps/settings/customize/customize_settings_model.cpp`
- `src/apps/settings/customize/customize_settings_projection.cpp`
- `src/apps/settings/customize/include/qindaqt/apps/settings_customize/customize_editor_host.h`
- `src/apps/settings/customize/include/qindaqt/apps/settings_customize/customize_settings_model.h`
- `src/apps/settings/customize/qml/CustomizeActionBar.qml`
- `src/apps/settings/customize/qml/CustomizeAppletPalette.qml`
- `src/apps/settings/customize/qml/CustomizeAppletProperties.qml`
- `src/apps/settings/customize/qml/CustomizeCanvas.qml`
- `src/apps/settings/customize/qml/CustomizeOutline.qml`
- `src/apps/settings/customize/qml/CustomizePage.qml`
- `src/apps/settings/customize/qml/CustomizePanelPositionProperties.qml`
- `src/apps/settings/customize/qml/CustomizePanelSizeProperties.qml`
- `src/apps/settings/customize/qml/CustomizePanelVisibilityProperties.qml`
- `src/apps/settings/customize/qml/CustomizeProperties.qml`
- `src/apps/settings/customize/qml/CustomizeRoute.qml`
- `src/apps/settings_center/CMakeLists.txt`
- `src/apps/settings_center/SettingsRouteHost.qml`
- `src/apps/settings_center/settings_route.cpp`
- `src/apps/settings_center/settings_route.h`
- `src/apps/settings_center/settings_route_registry.cpp`
- `tests/CMakeLists.txt`
- `tests/apps/settings/customize/CMakeLists.txt`
- `tests/apps/settings/customize/check_boundary.cmake`
- `tests/apps/settings/customize/check_boundary_negative.cmake`
- `tests/apps/settings/customize/customize_test_support.h`
- `tests/apps/settings/customize/stub_customize_settings_model.h`
- `tests/apps/settings/customize/tst_customize_page.cpp`
- `tests/apps/settings/customize/tst_customize_settings_model.cpp`
- `tests/apps/settings_center/CMakeLists.txt`
- `tests/apps/settings_center/check_installed_routes.cmake`
- `tests/apps/settings_center/check_route_construction.cmake`
- `tests/apps/settings_center/tst_settings_navigation_controller.cpp`
- `tests/apps/settings_center/tst_settings_route_registry.cpp`

## Acceptance evidence

- Exact mandated Debug configure command with build root `.../customize-settings-canvas/debug`: exit 0.
- Exact mandated Release configure command with build root `.../customize-settings-canvas/release`: exit 0.
- Focused Debug build of `qindaqt_settings_customize_model_tests`, `qindaqt_settings_customize_page_tests`, `qindaqt-settings`, the three Settings navigation targets, five `shell_customization` tests, and six `shell_customization_editor` tests: exit 0 under strict warnings.
- Same focused Release build target set: exit 0 under strict warnings.
- `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/debug -R 'customiz' --output-on-failure --no-tests=error`: exit 0, 16/16 passed.
- Same `customiz` selector in Release: exit 0, 16/16 passed.
- `ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/debug --output-on-failure --no-tests=error -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$'`: exit 0, 9/9 passed.
- Same Settings selector in Release: exit 0, 9/9 passed.
- `./tools/validate-docs`: exit 0, 117 Markdown documents plus navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/customize-settings-canvas/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 1,786 source files checked; only pre-existing warnings outside owned paths remain.
- `git diff --check`: exit 0 before the product commit.
- No JSON file changed, so no JSON parser gate applied.

## Bounded caveats

- This candidate deliberately does not claim a live session bus, host service, compositor, AT-SPI process, hardware/input device, nested session, or rendered session matrix.
- Applied profiles are durable and selected through Settings1 but are adopted by the production shell at its next start; provisional live-shell binding is a later lane.
- Always-hidden panel mode remains unavailable until the reveal-affordance lane lands.
- Manifest settings are truthfully projected read-only because the accepted public editor intents expose no arbitrary applet-settings mutation; this route does not bypass that boundary with direct engine commands.
- The installed tests prove a sanitized relocated package, not installed-session behavior.

## Requested next action

Independent exact review of `a6ea864ec2162dd5c1a8fba9f2d8f1f2f09a321f`, then manager integration.
