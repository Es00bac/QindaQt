# Hee Oh handoff — Bluetooth pairing and Agent1 UX

- Timestamp: `2026-09-03T12:02:05-06:00`
- Candidate commit: `43a7cb16d4d053b1e05ba4351d986b235676cbde`
- Candidate tree: `608c4ebcc882a8cb80f4f76a0a1a6a462ab49df4`
- Exact base: `196e69dc42d8761aa95b0f73cefd6ad8d0d25bf0`
- Branch/worktree: `worker/bluetooth-pairing` at `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-pairing`

## Outcome

Bluetooth1 now exposes exact-owner/epoch/revision-fenced Pair, CancelPairing, Remove, SetTrusted, and typed prompt replies. The injected production BlueZ adapter registers one `KeyboardDisplay` Agent1, rejects foreign callers, publishes one bounded 60-second prompt, and fails closed on cancellation, timeout, malformed input, and owner loss. Settings adds Pair/Forget/Trust and accessible inline confirmation/PIN/passkey handling; the applet truthfully presents the prompt with Confirm/Cancel. QindaQt stores no device records, pairing keys, trust decisions, or prompt input.

## Changed paths

- `docs/wiki/adr/0037-keep-pairing-and-trust-authority-in-bluez.md`
- `docs/wiki/adr/0057-reach-bluez-through-direct-qtdbus-behind-adapter-backend.md`
- `docs/wiki/apps/bluetooth-settings.md`
- `docs/wiki/architecture/bluetooth-service.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/bluetooth1-v1.md`
- `docs/wiki/shell/bluetooth-applet.md`
- `src/apps/settings/bluetooth/CMakeLists.txt`
- `src/apps/settings/bluetooth/bluetooth_settings_model.cpp`
- `src/apps/settings/bluetooth/bluetooth_settings_pairing.cpp`
- `src/apps/settings/bluetooth/include/qindaqt/apps/settings_bluetooth/bluetooth_settings_model.h`
- `src/apps/settings/bluetooth/qml/BluetoothDeviceSection.qml`
- `src/apps/settings/bluetooth/qml/BluetoothPage.qml`
- `src/apps/settings/bluetooth/qml/BluetoothPairingSection.qml`
- `src/services/bluetooth_bluez_adapter/CMakeLists.txt`
- `src/services/bluetooth_bluez_adapter/include/qindaqt/services/bluetooth_bluez_adapter/bluez_adapter_backend.h`
- `src/services/bluetooth_bluez_adapter/src/bluez_adapter_backend.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_adapter_backend_p.h`
- `src/services/bluetooth_bluez_adapter/src/bluez_adapter_publication.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_object_store.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_object_store.h`
- `src/services/bluetooth_bluez_adapter/src/bluez_operation_completion.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_pairing_agent.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_pairing_agent.h`
- `src/services/bluetooth_bluez_adapter/src/bluez_pairing_operations.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_transport.cpp`
- `src/services/bluetooth_bluez_adapter/src/bluez_transport.h`
- `src/services/bluetooth_client/CMakeLists.txt`
- `src/services/bluetooth_client/include/qindaqt/services/bluetooth_client/bluetooth_client.h`
- `src/services/bluetooth_client/src/bluetooth_client.cpp`
- `src/services/bluetooth_client/src/bluetooth_client_completion.cpp`
- `src/services/bluetooth_client/src/bluetooth_client_operations.cpp`
- `src/services/bluetooth_client/src/bluetooth_client_pairing.cpp`
- `src/services/bluetooth_client/src/qt_bluetooth_transport.cpp`
- `src/services/bluetooth_model/include/qindaqt/services/bluetooth_model/adapter_backend.h`
- `src/services/bluetooth_model/src/bluetooth_model.cpp`
- `src/services/bluetooth_model/src/bluetooth_model_publication.cpp`
- `src/services/bluetooth_model/src/deterministic_adapter_backend.cpp`
- `src/services/bluetooth_protocol/CMakeLists.txt`
- `src/services/bluetooth_protocol/include/qindaqt/services/bluetooth_protocol/bluetooth_dbus.h`
- `src/services/bluetooth_protocol/include/qindaqt/services/bluetooth_protocol/bluetooth_limits.h`
- `src/services/bluetooth_protocol/include/qindaqt/services/bluetooth_protocol/bluetooth_types.h`
- `src/services/bluetooth_protocol/src/bluetooth_dbus.cpp`
- `src/services/bluetooth_protocol/src/bluetooth_validation.cpp`
- `src/services/bluetooth_service/CMakeLists.txt`
- `src/services/bluetooth_service/data/org.qindaqt.Bluetooth1.xml`
- `src/services/bluetooth_service/src/bluetooth_service_object.cpp`
- `src/services/bluetooth_service/src/bluetooth_service_object_p.h`
- `src/shell/bluetooth_applet/CMakeLists.txt`
- `src/shell/bluetooth_applet/qml/BluetoothApplet.qml`
- `src/shell/bluetooth_applet/qml/BluetoothPairingPrompt.qml`
- `src/shell/bluetooth_applet/src/bluetooth_applet_controller.cpp`
- `src/shell/bluetooth_applet/src/bluetooth_applet_controller.h`
- `src/shell/bluetooth_applet/src/bluetooth_applet_pairing.cpp`
- `src/shell/bluetooth_applet/src/bluetooth_request_state.cpp`
- `tests/apps/settings/bluetooth/bluetooth_settings_test_support.h`
- `tests/apps/settings/bluetooth/check_boundary.cmake`
- `tests/apps/settings/bluetooth/check_boundary_negative.cmake`
- `tests/apps/settings/bluetooth/stub_bluetooth_settings_model.h`
- `tests/apps/settings/bluetooth/tst_bluetooth_page.cpp`
- `tests/apps/settings/bluetooth/tst_bluetooth_settings_model.cpp`
- `tests/services/bluetooth_bluez_adapter/CMakeLists.txt`
- `tests/services/bluetooth_bluez_adapter/check_boundary.cmake`
- `tests/services/bluetooth_bluez_adapter/check_boundary_negative.cmake`
- `tests/services/bluetooth_bluez_adapter/support/bluez_harness.h`
- `tests/services/bluetooth_bluez_adapter/support/fake_bluez.cpp`
- `tests/services/bluetooth_bluez_adapter/support/fake_bluez.h`
- `tests/services/bluetooth_bluez_adapter/support/fake_bluez_pairing.cpp`
- `tests/services/bluetooth_bluez_adapter/tst_bluez_pairing.cpp`
- `tests/services/bluetooth_client/support/fake_bluetooth_transport.h`
- `tests/services/bluetooth_client/tst_bluetooth_activation.cpp`
- `tests/services/bluetooth_client/tst_bluetooth_client.cpp`
- `tests/services/bluetooth_client/tst_qt_bluetooth_transport.cpp`
- `tests/services/bluetooth_protocol/tst_bluetooth_protocol.cpp`
- `tests/services/bluetooth_service/run_staged_install.cmake`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_controller.cpp`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_presentation.cpp`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_qml.cpp`
- `tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp`
- `tests/shell/bluetooth_applet/tst_bluetooth_request_state.cpp`

## Acceptance evidence

- Debug configure with the assigned system-KWin cache and strict warnings: exit 0.
  `cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-pairing/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`
- Release configure with the same cache/flags and `-DCMAKE_BUILD_TYPE=Release`: exit 0.
- Focused Debug build of Bluetooth protocol/model/client/service/BlueZ tests, Settings Bluetooth tests, applet tests, `qindaqt-settings`, and `qindaqt-shell`: exit 0. The final post-audit rebuild of `qindaqt_bluetooth_activation_tests qindaqt_bluez_pairing_tests` also exited 0.
- Focused Release build of the same owned tests and dependency-adjacent consumers (1,281 Ninja actions in the fresh tree): exit 0. The final post-audit rebuild of the activation/pairing targets also exited 0.
- Final Debug `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir .../debug -R '^qindaqt\.(bluetooth-|settings-bluetooth-)' --output-on-failure --no-tests=error`: exit 0, 31/31 passed.
- Final Debug `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir .../debug -R 'bluetooth' --output-on-failure --no-tests=error`: exit 0, 31/31 passed.
- Final Release equivalents for both selectors: exit 0, 31/31 passed for each.
- Debug and Release warning-fatal offscreen selector `QT_FATAL_WARNINGS=1 ... -R '^(qindaqt\.settings-bluetooth-(page|window-close)|qindaqt\.bluetooth-applet-(offscreen|surface))$'`: exit 0, 4/4 passed in each configuration.
- Final private-bus activation/pairing recheck `-R '^qindaqt\.bluetooth-(activation|bluez-pairing)$'`: exit 0, 2/2 passed in Debug and 2/2 in Release. This includes the foreign-Agent1-caller rejection and confines activation runtime files below each build tree.
- `./tools/validate-docs`: exit 0, 139 Markdown documents/navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-pairing/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 2,419 files checked; all owned production sources remain below the hard limits.
- `git diff --check` and staged `git diff --cached --check`: exit 0.
- No JSON changed, so no JSON parser gate applied.
- Repair-loop disclosure: the first Debug selector run exited 8 with 24/31 passed (two test executables and three installed consumers were not yet built, plus two stale boundary policies). After building consumers and updating the boundary policies, 30/31 passed; the remaining pre-existing whole-install staging design failed twice on unrelated unbuilt task-list archives. Scoping the Bluetooth staged-install row to its existing `QindaQtBluetoothB1` component fixed that condition; every final matrix above is green.

## Bounded caveats

This candidate proves the BlueZ contract only against an injected fake `org.bluez` on private buses. It does not claim host BlueZ, physical-radio/device interoperability, suspend/hotplug, Bluetooth audio routing, live AT-SPI traversal, or nested-compositor evidence. The applet deliberately does not accept PIN/passkey text; it shows that prompt truthfully and offers Confirm/Cancel as assigned.

## Requested next action

Independent exact review then manager integration.
