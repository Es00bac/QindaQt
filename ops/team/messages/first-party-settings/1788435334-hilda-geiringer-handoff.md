# Power Settings route candidate handoff — Hilda Geiringer

- Feature: QQ-006.05 Settings pages and live platform-service routes (Power and brightness page).
- Exact candidate commit: `13a0870433f5a101af9a5d860a9690599ba5391f`.
- Candidate tree: `8869e2270cbefbe61c6bed7c9301d850387fdc1b`.
- Exact base: `347d32f92b32c57edd4fe055c26aab8a8f27298e`.
- Branch: `worker/power-settings-route`.

## Outcome

The candidate adds installed `qindaqt-settings --page power` as append-only route eight with Ctrl+8. It composes only the public Power client, projects bounded supply/profile/hold/internal- and keyboard-brightness truth, shares exact owner/epoch/revision admission between displayed actions and dispatch, debounces keyboard slider bursts, fences one pending/convergence operation, and has no session-action authority. The QST/Controls page is keyboard-accessible in wide and compact hosts. The statically linked public-client composition is separated from the shared installed page module so relocation failure and success are both observable.

## Changed paths

- `docs/wiki/apps/power-settings.md`
- `docs/wiki/apps/settings-center.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/architecture/power-service.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/apps/settings/power/CMakeLists.txt`
- `src/apps/settings/power/include/qindaqt/apps/settings_power/power_settings_model.h`
- `src/apps/settings/power/power_route_composition.cpp`
- `src/apps/settings/power/power_route_composition.h`
- `src/apps/settings/power/power_settings_model.cpp`
- `src/apps/settings/power/power_settings_projection.cpp`
- `src/apps/settings/power/power_settings_projection.h`
- `src/apps/settings/power/qml/PowerBrightnessSection.qml`
- `src/apps/settings/power/qml/PowerPage.qml`
- `src/apps/settings/power/qml/PowerProfileSection.qml`
- `src/apps/settings/power/qml/PowerSupplySection.qml`
- `src/apps/settings_center/CMakeLists.txt`
- `src/apps/settings_center/Main.qml`
- `src/apps/settings_center/SettingsRouteHost.qml`
- `src/apps/settings_center/settings_route.cpp`
- `src/apps/settings_center/settings_route.h`
- `src/apps/settings_center/settings_route_registry.cpp`
- `tests/CMakeLists.txt`
- `tests/apps/settings/power/CMakeLists.txt`
- `tests/apps/settings/power/check_boundary.cmake`
- `tests/apps/settings/power/check_boundary_negative.cmake`
- `tests/apps/settings/power/check_installed_route.cmake`
- `tests/apps/settings/power/power_navigation_assertions.h`
- `tests/apps/settings/power/power_settings_test_support.h`
- `tests/apps/settings/power/stub_power_settings_model.h`
- `tests/apps/settings/power/tst_power_page.cpp`
- `tests/apps/settings/power/tst_power_settings_model.cpp`
- `tests/apps/settings/power/tst_power_settings_slider.cpp`
- `tests/apps/settings_center/CMakeLists.txt`
- `tests/apps/settings_center/check_installed_routes.cmake`
- `tests/apps/settings_center/check_route_construction.cmake`
- `tests/apps/settings_center/tst_settings_navigation_controller.cpp`
- `tests/apps/settings_center/tst_settings_navigation_page.cpp`
- `tests/apps/settings_center/tst_settings_route_registry.cpp`

## Acceptance evidence

All commands below ran from the exact candidate worktree.

- Debug configure, exact prescribed recipe with build root `/home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/debug`: exit 0.
- Release configure, exact prescribed recipe with build root `/home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/release`: exit 0.
- `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/debug --parallel 3 --target qindaqt_power_settings_model_tests qindaqt_power_settings_slider_tests qindaqt_power_page_tests qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test qindaqt-settings`: exit 0 (final incremental 13/13).
- `cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/release --parallel 3 --target qindaqt_power_settings_model_tests qindaqt_power_settings_slider_tests qindaqt_power_page_tests qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test qindaqt-settings`: exit 0 (658/658 fresh build steps).
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/debug -R '^qindaqt\.settings-power-' --output-on-failure --no-tests=error`: exit 0, 6/6 passed.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/release -R '^qindaqt\.settings-power-' --output-on-failure --no-tests=error`: exit 0, 6/6 passed.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/debug -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' --output-on-failure --no-tests=error`: exit 0, 9/9 passed.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/release -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' --output-on-failure --no-tests=error`: exit 0, 9/9 passed.
- `./tools/validate-docs`: exit 0, 130 Markdown documents plus navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/power-settings-route/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2101 files checked and 0 skipped. It retained non-failing decomposition-review warnings for shared files already above the review threshold; Power-specific host assertions live in the owned helper rather than enlarging the shared test with their implementation.
- `git diff --check`: exit 0.
- No JSON changed, so no JSON parser command applied.

## Bounded caveats

- This candidate deliberately does not claim or add suspend, hibernate, restart, power-off, lock, profile-hold mutation, internal-display mutation, charge thresholds, or persistence.
- Verification used injected fake transport and absent private buses only. It does not claim host D-Bus, UPower, power-profiles-daemon, logind, sysfs, hardware, Wayland, live AT-SPI, physical brightness keys, or nested-session evidence.
- The existing shared Settings navigation page test remains above the source-shape decomposition-review threshold but below its enforced maximum; the candidate adds only compact calls and moves Power-specific assertions to `tests/apps/settings/power/power_navigation_assertions.h`.

## Requested next action

Independent exact review of candidate `13a0870433f5a101af9a5d860a9690599ba5391f`, then manager integration.
