# Bluetooth Settings route candidate handoff

- Candidate commit: `bf7b00fec5a80f3d37795568d4dde6c35a72ea19`
- Candidate tree: `b64c3c8ecaa101a5f0ef2d30acef8c5f5de3caed`
- Exact base: `ee187e97221ee7f13d6e4e00e6ee3356b6faf3d8`
- Branch: `worker/bluetooth-settings-route`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-settings-route`

## User-visible outcome

The installed canonical `qindaqt-settings --page bluetooth` route now projects only the public Bluetooth1 client. It presents bounded address-free adapter/device truth, semantic class icons and RSSI, power plus route-scoped discovery, and paired-device connect/disconnect under one exact owner/epoch/revision admission predicate. Route departure releases its single caller-scoped discovery lease; window close waits for an admitted acquire/release result, with Bluetooth1 caller-disappearance cleanup remaining the unavailable-authority guarantee. The page visibly excludes pairing, trust, untrust, and removal authority and remains keyboard/accessibility complete in wide and compact QST/Controls layouts.

## Changed paths

- `docs/wiki/apps/bluetooth-settings.md`
- `docs/wiki/apps/settings-center.md`
- `docs/wiki/architecture/bluetooth-service.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/index.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/apps/settings/bluetooth/CMakeLists.txt`
- `src/apps/settings/bluetooth/bluetooth_settings_model.cpp`
- `src/apps/settings/bluetooth/bluetooth_settings_projection.cpp`
- `src/apps/settings/bluetooth/bluetooth_settings_projection.h`
- `src/apps/settings/bluetooth/include/qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h`
- `src/apps/settings/bluetooth/qml/BluetoothAdapterSection.qml`
- `src/apps/settings/bluetooth/qml/BluetoothDeviceSection.qml`
- `src/apps/settings/bluetooth/qml/BluetoothPage.qml`
- `src/apps/settings_center/CMakeLists.txt`
- `src/apps/settings_center/Main.qml`
- `src/apps/settings_center/SettingsRouteHost.qml`
- `src/apps/settings_center/main.cpp`
- `src/apps/settings_center/settings_route.cpp`
- `src/apps/settings_center/settings_route.h`
- `src/apps/settings_center/settings_route_registry.cpp`
- `tests/CMakeLists.txt`
- `tests/apps/settings/bluetooth/CMakeLists.txt`
- `tests/apps/settings/bluetooth/bluetooth_settings_test_support.h`
- `tests/apps/settings/bluetooth/check_boundary.cmake`
- `tests/apps/settings/bluetooth/check_boundary_negative.cmake`
- `tests/apps/settings/bluetooth/check_installed_route.cmake`
- `tests/apps/settings/bluetooth/stub_bluetooth_settings_model.h`
- `tests/apps/settings/bluetooth/tst_bluetooth_page.cpp`
- `tests/apps/settings/bluetooth/tst_bluetooth_settings_model.cpp`
- `tests/apps/settings/bluetooth/tst_bluetooth_window_close.cpp`
- `tests/apps/settings_center/CMakeLists.txt`
- `tests/apps/settings_center/check_installed_routes.cmake`
- `tests/apps/settings_center/check_route_construction.cmake`
- `tests/apps/settings_center/tst_settings_navigation_controller.cpp`
- `tests/apps/settings_center/tst_settings_navigation_page.cpp`
- `tests/apps/settings_center/tst_settings_route_registry.cpp`

## Acceptance evidence

All commands below exited 0 on the candidate tree.

1. Debug configure:

   ```sh
   cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-settings-route/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
   ```

2. Release configure: the same command with `-B /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-settings-route/release -DCMAKE_BUILD_TYPE=Release`.

3. Focused build, once per Debug and Release profile:

   ```sh
   cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-settings-route/<profile> --parallel 3 --target qindaqt_settings_bluetooth qindaqt_settings_bluetooth_qml qindaqt_bluetooth_settings_model_tests qindaqt_bluetooth_page_tests qindaqt_bluetooth_window_close_tests qindaqt-settings qindaqt_settings_route_registry_test qindaqt_settings_navigation_controller_test qindaqt_settings_navigation_page_test
   ```

4. Bluetooth selector, once per Debug and Release profile: exit 0, **6/6 passed** in each profile.

   ```sh
   DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-settings-route/<profile> -R '^qindaqt\.settings-bluetooth-' --output-on-failure --no-tests=error
   ```

5. Settings Center selector, once per Debug and Release profile: exit 0, **9/9 passed** in each profile.

   ```sh
   DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-settings-route/<profile> --output-on-failure --no-tests=error -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$'
   ```

6. Static gates:

   ```sh
   ./tools/validate-docs
   /home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-settings-route/site
   ./tools/check-source-shape
   git diff --check
   ```

   Results: exit 0; 126 Markdown documents/navigation validated; strict MkDocs built successfully; 1,994 source files checked with no new size violation (the largest changed shared test is 499 non-blank lines); whitespace check clean. No JSON was changed, so no JSON formatter gate applied.

Superseded repair-loop diagnostics found and repaired three non-acceptance failures: a model-helper name collision, a warning-fatal invalid `GridLayout.spacing` property, and an unused private translation helper after projection decomposition. An initial Settings history expectation and installed warning policy were also corrected. Every affected selector and build was rerun from the repaired tree; only the green reruns above are acceptance evidence.

## Bounded caveats

- This candidate deliberately claims no host session/system bus, BlueZ, radio, rfkill, Bluetooth audio, hardware, uinput, live AT-SPI, screen-reader, nested compositor, or nested-session evidence.
- Pairing, trust, untrust, removal, keys, authorization, and Agent1 remain BlueZ authority and are absent from the route API.
- A failed or uncertain explicit discovery release is not replayed. On process close, Bluetooth1's already-qualified unique-caller disappearance contract releases remaining leases; on route-only departure the model retains truthful lease state and retries only after authoritative mutation eligibility returns.
- The focused configure emits the repository's pre-existing mixed-Qt runtime-search-path warnings; strict warnings-as-errors compilation and every owned selector still pass.

## Requested next action

Independent exact review then manager integration.
